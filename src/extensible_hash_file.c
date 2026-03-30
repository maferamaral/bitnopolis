#include "../include/extensible_hash_file.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EHF_MAGIC 0x45484631u /* "EHF1" */
#define EHF_VERSION 1u
#define EHF_MAX_KEY_LEN 63u
#define EHF_HASH_SEED 1469598103934665603ULL
#define EHF_HASH_PRIME 1099511628211ULL

/*
 * Implementacao de um hash file extensivel em disco.
 *
 * Arquivos gerados:
 *  - <nome>.hf  : arquivo binario principal
 *  - <nome>.hfd : representacao textual simplificada do estado atual
 *
 * O header publico mantem o TAD opaco. Aqui a estrutura real fica escondida.
 */

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t bucket_capacity;
    uint32_t global_depth;
    uint32_t bucket_count;
    uint32_t directory_size;
    uint64_t next_bucket_offset;
} EHFFileHeader;

typedef struct
{
    uint32_t local_depth;
    uint32_t record_count;
    uint32_t bucket_capacity;
    uint32_t reserved;
} EHFBucketHeader;

typedef struct
{
    uint8_t in_use;
    uint8_t key_len;
    uint16_t value_size;
    char key[EHF_MAX_KEY_LEN + 1];
} EHFRecordHeader;

typedef struct
{
    FILE *fp;
    char *filename;
    size_t value_size_max_seen;
    EHFFileHeader hdr;
    uint64_t *directory;
} ExtensibleHashFile;

static int ehf_flush_metadata(ExtensibleHashFile *hf);
static int ehf_write_hfd(ExtensibleHashFile *hf, const char *event_message);
static uint64_t ehf_hash_key(const char *key);
static uint32_t ehf_directory_index(const ExtensibleHashFile *hf, const char *key);
static size_t ehf_bucket_size_bytes(uint32_t bucket_capacity);
static int ehf_read_bucket_header(ExtensibleHashFile *hf, uint64_t bucket_off, EHFBucketHeader *bh);
static int ehf_write_bucket_header(ExtensibleHashFile *hf, uint64_t bucket_off, const EHFBucketHeader *bh);
static int ehf_read_record(ExtensibleHashFile *hf, uint64_t bucket_off, uint32_t slot, EHFRecordHeader *rh, void *value_out);
static int ehf_write_record(ExtensibleHashFile *hf, uint64_t bucket_off, uint32_t slot, const EHFRecordHeader *rh, const void *value);
static int ehf_clear_slot(ExtensibleHashFile *hf, uint64_t bucket_off, uint32_t slot);
static int ehf_find_slot(ExtensibleHashFile *hf, uint64_t bucket_off, const char *key, uint32_t *found_slot, int *exists, uint32_t *free_slot);
static int ehf_bucket_insert_raw(ExtensibleHashFile *hf, uint64_t bucket_off, const char *key, const void *value, size_t value_size);
static int ehf_bucket_collect(ExtensibleHashFile *hf, uint64_t bucket_off,
                              char keys[][EHF_MAX_KEY_LEN + 1],
                              uint16_t *value_sizes,
                              unsigned char *values,
                              uint32_t *count);
static int ehf_bucket_reset(ExtensibleHashFile *hf, uint64_t bucket_off, uint32_t local_depth);
static int ehf_double_directory(ExtensibleHashFile *hf);
static int ehf_split_bucket(ExtensibleHashFile *hf, uint32_t dir_index, const char *reason_key);
static int ehf_validate_handle(ExtensibleHashFile *hf);
static int ehf_update_value_size_max_seen(ExtensibleHashFile *hf, size_t value_size);

static char *ehf_strdup_local(const char *s)
{
    size_t n;
    char *copy;

    if (s == NULL)
    {
        return NULL;
    }

    n = strlen(s);
    copy = (char *)malloc(n + 1);
    if (copy == NULL)
    {
        return NULL;
    }
    memcpy(copy, s, n + 1);
    return copy;
}

static int ehf_validate_key(const char *key)
{
    size_t len;

    if (key == NULL)
    {
        return 0;
    }

    len = strlen(key);
    return len > 0 && len <= EHF_MAX_KEY_LEN;
}

static size_t ehf_bucket_size_bytes(uint32_t bucket_capacity)
{
    return sizeof(EHFBucketHeader) +
           ((size_t)bucket_capacity * (sizeof(EHFRecordHeader) + sizeof(uint16_t)));
}

static uint64_t ehf_hash_key(const char *key)
{
    uint64_t h = EHF_HASH_SEED;
    const unsigned char *p = (const unsigned char *)key;

    while (*p != '\0')
    {
        h ^= (uint64_t)(*p);
        h *= EHF_HASH_PRIME;
        p++;
    }

    return h;
}

static uint32_t ehf_directory_index(const ExtensibleHashFile *hf, const char *key)
{
    uint64_t h;
    uint32_t mask;

    assert(hf != NULL);
    assert(hf->hdr.global_depth < 32u);

    h = ehf_hash_key(key);
    if (hf->hdr.global_depth == 0u)
    {
        return 0u;
    }

    mask = (1u << hf->hdr.global_depth) - 1u;
    return (uint32_t)(h & (uint64_t)mask);
}

static uint64_t ehf_bucket_record_offset(uint64_t bucket_off, uint32_t slot)
{
    return bucket_off + sizeof(EHFBucketHeader) +
           ((uint64_t)slot * (uint64_t)(sizeof(EHFRecordHeader) + sizeof(uint16_t)));
}

static int ehf_read_bucket_header(ExtensibleHashFile *hf, uint64_t bucket_off, EHFBucketHeader *bh)
{
    if (fseek(hf->fp, (long)bucket_off, SEEK_SET) != 0)
    {
        return 0;
    }
    return fread(bh, sizeof(EHFBucketHeader), 1, hf->fp) == 1;
}

static int ehf_write_bucket_header(ExtensibleHashFile *hf, uint64_t bucket_off, const EHFBucketHeader *bh)
{
    if (fseek(hf->fp, (long)bucket_off, SEEK_SET) != 0)
    {
        return 0;
    }
    if (fwrite(bh, sizeof(EHFBucketHeader), 1, hf->fp) != 1)
    {
        return 0;
    }
    return fflush(hf->fp) == 0;
}

static int ehf_read_record(ExtensibleHashFile *hf, uint64_t bucket_off, uint32_t slot, EHFRecordHeader *rh, void *value_out)
{
    uint64_t off = ehf_bucket_record_offset(bucket_off, slot);
    uint16_t stored_size;

    if (fseek(hf->fp, (long)off, SEEK_SET) != 0)
    {
        return 0;
    }
    if (fread(rh, sizeof(EHFRecordHeader), 1, hf->fp) != 1)
    {
        return 0;
    }
    if (fread(&stored_size, sizeof(uint16_t), 1, hf->fp) != 1)
    {
        return 0;
    }

    if (rh->in_use && value_out != NULL && stored_size > 0)
    {
        if (fread(value_out, stored_size, 1, hf->fp) != 1)
        {
            return 0;
        }
    }

    return 1;
}

static int ehf_write_record(ExtensibleHashFile *hf, uint64_t bucket_off, uint32_t slot, const EHFRecordHeader *rh, const void *value)
{
    uint64_t off = ehf_bucket_record_offset(bucket_off, slot);
    uint16_t stored_size = rh->value_size;

    if (fseek(hf->fp, (long)off, SEEK_SET) != 0)
    {
        return 0;
    }
    if (fwrite(rh, sizeof(EHFRecordHeader), 1, hf->fp) != 1)
    {
        return 0;
    }
    if (fwrite(&stored_size, sizeof(uint16_t), 1, hf->fp) != 1)
    {
        return 0;
    }
    if (rh->in_use && stored_size > 0)
    {
        if (fwrite(value, stored_size, 1, hf->fp) != 1)
        {
            return 0;
        }
    }
    return fflush(hf->fp) == 0;
}

static int ehf_clear_slot(ExtensibleHashFile *hf, uint64_t bucket_off, uint32_t slot)
{
    EHFRecordHeader rh;
    unsigned char zero = 0;
    uint64_t off = ehf_bucket_record_offset(bucket_off, slot);
    size_t payload_bytes = sizeof(EHFRecordHeader) + sizeof(uint16_t);
    size_t i;

    memset(&rh, 0, sizeof(rh));

    if (fseek(hf->fp, (long)off, SEEK_SET) != 0)
    {
        return 0;
    }
    for (i = 0; i < payload_bytes; i++)
    {
        if (fwrite(&zero, sizeof(unsigned char), 1, hf->fp) != 1)
        {
            return 0;
        }
    }
    return fflush(hf->fp) == 0;
}

static int ehf_find_slot(ExtensibleHashFile *hf, uint64_t bucket_off, const char *key, uint32_t *found_slot, int *exists, uint32_t *free_slot)
{
    EHFBucketHeader bh;
    uint32_t i;

    *exists = 0;
    *found_slot = 0u;
    *free_slot = UINT32_MAX;

    if (!ehf_read_bucket_header(hf, bucket_off, &bh))
    {
        return 0;
    }

    for (i = 0; i < bh.bucket_capacity; i++)
    {
        EHFRecordHeader rh;

        if (!ehf_read_record(hf, bucket_off, i, &rh, NULL))
        {
            return 0;
        }

        if (rh.in_use)
        {
            if (strcmp(rh.key, key) == 0)
            {
                *exists = 1;
                *found_slot = i;
                return 1;
            }
        }
        else if (*free_slot == UINT32_MAX)
        {
            *free_slot = i;
        }
    }

    return 1;
}

static int ehf_bucket_insert_raw(ExtensibleHashFile *hf, uint64_t bucket_off, const char *key, const void *value, size_t value_size)
{
    EHFBucketHeader bh;
    uint32_t found_slot;
    uint32_t free_slot;
    int exists;
    EHFRecordHeader rh;

    if (!ehf_read_bucket_header(hf, bucket_off, &bh))
    {
        return -1;
    }

    if (!ehf_find_slot(hf, bucket_off, key, &found_slot, &exists, &free_slot))
    {
        return -1;
    }

    if (exists)
    {
        return 0;
    }
    if (free_slot == UINT32_MAX)
    {
        return -2;
    }

    memset(&rh, 0, sizeof(rh));
    rh.in_use = 1u;
    rh.key_len = (uint8_t)strlen(key);
    rh.value_size = (uint16_t)value_size;
    memcpy(rh.key, key, rh.key_len + 1u);

    if (!ehf_write_record(hf, bucket_off, free_slot, &rh, value))
    {
        return -1;
    }

    bh.record_count++;
    if (!ehf_write_bucket_header(hf, bucket_off, &bh))
    {
        return -1;
    }

    return 1;
}

static int ehf_bucket_collect(ExtensibleHashFile *hf, uint64_t bucket_off,
                              char keys[][EHF_MAX_KEY_LEN + 1],
                              uint16_t *value_sizes,
                              unsigned char *values,
                              uint32_t *count)
{
    EHFBucketHeader bh;
    uint32_t i;
    uint32_t n = 0u;

    if (!ehf_read_bucket_header(hf, bucket_off, &bh))
    {
        return 0;
    }

    for (i = 0; i < bh.bucket_capacity; i++)
    {
        EHFRecordHeader rh;
        unsigned char *dst;

        if (!ehf_read_record(hf, bucket_off, i, &rh, NULL))
        {
            return 0;
        }

        if (!rh.in_use)
        {
            continue;
        }

        dst = values + ((size_t)n * (size_t)hf->value_size_max_seen);
        if (!ehf_read_record(hf, bucket_off, i, &rh, dst))
        {
            return 0;
        }

        strcpy(keys[n], rh.key);
        value_sizes[n] = rh.value_size;
        n++;
    }

    *count = n;
    return 1;
}

static int ehf_bucket_reset(ExtensibleHashFile *hf, uint64_t bucket_off, uint32_t local_depth)
{
    EHFBucketHeader bh;
    uint32_t i;

    memset(&bh, 0, sizeof(bh));
    bh.local_depth = local_depth;
    bh.record_count = 0u;
    bh.bucket_capacity = hf->hdr.bucket_capacity;

    if (!ehf_write_bucket_header(hf, bucket_off, &bh))
    {
        return 0;
    }

    for (i = 0; i < hf->hdr.bucket_capacity; i++)
    {
        if (!ehf_clear_slot(hf, bucket_off, i))
        {
            return 0;
        }
    }

    return 1;
}

static int ehf_flush_metadata(ExtensibleHashFile *hf)
{
    if (fseek(hf->fp, 0L, SEEK_SET) != 0)
    {
        return 0;
    }
    if (fwrite(&hf->hdr, sizeof(hf->hdr), 1, hf->fp) != 1)
    {
        return 0;
    }
    if (fwrite(hf->directory, sizeof(uint64_t), hf->hdr.directory_size, hf->fp) != hf->hdr.directory_size)
    {
        return 0;
    }
    return fflush(hf->fp) == 0;
}

static int ehf_double_directory(ExtensibleHashFile *hf)
{
    uint32_t old_size = hf->hdr.directory_size;
    uint32_t new_size;
    uint64_t *new_dir;
    uint32_t i;

    if (hf->hdr.global_depth >= 31u)
    {
        return 0;
    }

    new_size = old_size * 2u;
    new_dir = (uint64_t *)malloc((size_t)new_size * sizeof(uint64_t));
    if (new_dir == NULL)
    {
        return 0;
    }

    for (i = 0; i < old_size; i++)
    {
        new_dir[i] = hf->directory[i];
        new_dir[i + old_size] = hf->directory[i];
    }

    free(hf->directory);
    hf->directory = new_dir;
    hf->hdr.directory_size = new_size;
    hf->hdr.global_depth++;

    return ehf_flush_metadata(hf);
}

static int ehf_update_value_size_max_seen(ExtensibleHashFile *hf, size_t value_size)
{
    if (value_size > hf->value_size_max_seen)
    {
        hf->value_size_max_seen = value_size;
    }
    return 1;
}

static int ehf_split_bucket(ExtensibleHashFile *hf, uint32_t dir_index, const char *reason_key)
{
    uint64_t old_bucket_off = hf->directory[dir_index];
    EHFBucketHeader old_bh;
    uint64_t new_bucket_off;
    char (*keys)[EHF_MAX_KEY_LEN + 1] = NULL;
    uint16_t *sizes = NULL;
    unsigned char *values = NULL;
    uint32_t record_count = 0u;
    uint32_t i;
    char event_msg[256];

    if (!ehf_read_bucket_header(hf, old_bucket_off, &old_bh))
    {
        return 0;
    }

    if (old_bh.local_depth == hf->hdr.global_depth)
    {
        if (!ehf_double_directory(hf))
        {
            return 0;
        }
    }

    keys = (char (*)[EHF_MAX_KEY_LEN + 1]) calloc(hf->hdr.bucket_capacity, sizeof(*keys));
    sizes = (uint16_t *)calloc(hf->hdr.bucket_capacity, sizeof(uint16_t));
    values = (unsigned char *)calloc((size_t)hf->hdr.bucket_capacity, hf->value_size_max_seen);
    if (keys == NULL || sizes == NULL || values == NULL)
    {
        free(keys);
        free(sizes);
        free(values);
        return 0;
    }

    if (!ehf_bucket_collect(hf, old_bucket_off, keys, sizes, values, &record_count))
    {
        free(keys);
        free(sizes);
        free(values);
        return 0;
    }

    new_bucket_off = hf->hdr.next_bucket_offset;
    hf->hdr.next_bucket_offset += (uint64_t)ehf_bucket_size_bytes(hf->hdr.bucket_capacity);
    hf->hdr.bucket_count++;

    if (!ehf_bucket_reset(hf, old_bucket_off, old_bh.local_depth + 1u) ||
        !ehf_bucket_reset(hf, new_bucket_off, old_bh.local_depth + 1u))
    {
        free(keys);
        free(sizes);
        free(values);
        return 0;
    }

    for (i = 0; i < hf->hdr.directory_size; i++)
    {
        if (hf->directory[i] == old_bucket_off)
        {
            if (((i >> old_bh.local_depth) & 1u) != 0u)
            {
                hf->directory[i] = new_bucket_off;
            }
        }
    }

    if (!ehf_flush_metadata(hf))
    {
        free(keys);
        free(sizes);
        free(values);
        return 0;
    }

    for (i = 0; i < record_count; i++)
    {
        uint32_t new_index = ehf_directory_index(hf, keys[i]);
        uint64_t target_off = hf->directory[new_index];
        if (ehf_bucket_insert_raw(hf, target_off, keys[i], values + ((size_t)i * hf->value_size_max_seen), sizes[i]) <= 0)
        {
            free(keys);
            free(sizes);
            free(values);
            return 0;
        }
    }

    snprintf(event_msg, sizeof(event_msg),
             "split bucket offset=%llu due_to_key=%s new_global_depth=%u bucket_count=%u",
             (unsigned long long)old_bucket_off,
             reason_key != NULL ? reason_key : "(unknown)",
             hf->hdr.global_depth,
             hf->hdr.bucket_count);
    (void)ehf_write_hfd(hf, event_msg);

    free(keys);
    free(sizes);
    free(values);
    return 1;
}

static int ehf_validate_handle(ExtensibleHashFile *hf)
{
    return hf != NULL && hf->fp != NULL && hf->directory != NULL;
}

static int ehf_write_hfd(ExtensibleHashFile *hf, const char *event_message)
{
    char *dump_name;
    size_t base_len;
    FILE *out;
    uint32_t i;

    if (!ehf_validate_handle(hf))
    {
        return 0;
    }

    base_len = strlen(hf->filename);
    dump_name = (char *)malloc(base_len + 5u);
    if (dump_name == NULL)
    {
        return 0;
    }

    strcpy(dump_name, hf->filename);
    strcat(dump_name, "d"); /* .hf -> .hfd */

    out = fopen(dump_name, "w");
    free(dump_name);
    if (out == NULL)
    {
        return 0;
    }

    fprintf(out, "HASH FILE EXTENSIVEL\n");
    fprintf(out, "arquivo=%s\n", hf->filename);
    fprintf(out, "bucket_capacity=%u\n", hf->hdr.bucket_capacity);
    fprintf(out, "global_depth=%u\n", hf->hdr.global_depth);
    fprintf(out, "bucket_count=%u\n", hf->hdr.bucket_count);
    fprintf(out, "directory_size=%u\n", hf->hdr.directory_size);
    if (event_message != NULL)
    {
        fprintf(out, "evento=%s\n", event_message);
    }
    fprintf(out, "\nDIRETORIO\n");

    for (i = 0; i < hf->hdr.directory_size; i++)
    {
        fprintf(out, "[%u] -> bucket_offset=%llu\n", i, (unsigned long long)hf->directory[i]);
    }

    fprintf(out, "\nBUCKETS\n");
    for (i = 0; i < hf->hdr.directory_size; i++)
    {
        uint64_t off = hf->directory[i];
        uint32_t j;
        int first_occurrence = 1;
        EHFBucketHeader bh;

        for (j = 0; j < i; j++)
        {
            if (hf->directory[j] == off)
            {
                first_occurrence = 0;
                break;
            }
        }
        if (!first_occurrence)
        {
            continue;
        }

        if (!ehf_read_bucket_header(hf, off, &bh))
        {
            continue;
        }

        fprintf(out, "bucket offset=%llu local_depth=%u count=%u\n",
                (unsigned long long)off,
                bh.local_depth,
                bh.record_count);

        for (j = 0; j < bh.bucket_capacity; j++)
        {
            EHFRecordHeader rh;
            if (!ehf_read_record(hf, off, j, &rh, NULL))
            {
                continue;
            }
            if (rh.in_use)
            {
                fprintf(out, "  slot=%u key=%s value_size=%u\n", j, rh.key, (unsigned)rh.value_size);
            }
            else
            {
                fprintf(out, "  slot=%u LIVRE\n", j);
            }
        }
    }

    fclose(out);
    return 1;
}

ExtHash ehf_create(const char *filename, size_t bucket_size)
{
    ExtensibleHashFile *hf;
    uint64_t initial_bucket_off;
    uint32_t i;

    if (filename == NULL || bucket_size == 0u)
    {
        return NULL;
    }

    hf = (ExtensibleHashFile *)calloc(1, sizeof(*hf));
    if (hf == NULL)
    {
        return NULL;
    }

    hf->filename = ehf_strdup_local(filename);
    if (hf->filename == NULL)
    {
        free(hf);
        return NULL;
    }

    hf->fp = fopen(filename, "w+b");
    if (hf->fp == NULL)
    {
        free(hf->filename);
        free(hf);
        return NULL;
    }

    hf->hdr.magic = EHF_MAGIC;
    hf->hdr.version = EHF_VERSION;
    hf->hdr.bucket_capacity = (uint32_t)bucket_size;
    hf->hdr.global_depth = 1u;
    hf->hdr.bucket_count = 2u;
    hf->hdr.directory_size = 2u;
    hf->hdr.next_bucket_offset = sizeof(EHFFileHeader) + (2u * sizeof(uint64_t));
    hf->value_size_max_seen = 1u;

    hf->directory = (uint64_t *)malloc((size_t)hf->hdr.directory_size * sizeof(uint64_t));
    if (hf->directory == NULL)
    {
        fclose(hf->fp);
        free(hf->filename);
        free(hf);
        return NULL;
    }

    initial_bucket_off = hf->hdr.next_bucket_offset;
    for (i = 0; i < hf->hdr.directory_size; i++)
    {
        hf->directory[i] = initial_bucket_off + ((uint64_t)i * (uint64_t)ehf_bucket_size_bytes(hf->hdr.bucket_capacity));
    }
    hf->hdr.next_bucket_offset += (uint64_t)hf->hdr.bucket_count * (uint64_t)ehf_bucket_size_bytes(hf->hdr.bucket_capacity);

    if (!ehf_flush_metadata(hf))
    {
        ehf_close(hf);
        return NULL;
    }

    for (i = 0; i < hf->hdr.bucket_count; i++)
    {
        if (!ehf_bucket_reset(hf, hf->directory[i], 1u))
        {
            ehf_close(hf);
            return NULL;
        }
    }

    (void)ehf_write_hfd(hf, "create");
    return (ExtHash)hf;
}

ExtHash ehf_open(const char *filename)
{
    ExtensibleHashFile *hf;

    if (filename == NULL)
    {
        return NULL;
    }

    hf = (ExtensibleHashFile *)calloc(1, sizeof(*hf));
    if (hf == NULL)
    {
        return NULL;
    }

    hf->filename = ehf_strdup_local(filename);
    if (hf->filename == NULL)
    {
        free(hf);
        return NULL;
    }

    hf->fp = fopen(filename, "r+b");
    if (hf->fp == NULL)
    {
        free(hf->filename);
        free(hf);
        return NULL;
    }

    if (fread(&hf->hdr, sizeof(hf->hdr), 1, hf->fp) != 1)
    {
        ehf_close((ExtHash)hf);
        return NULL;
    }

    if (hf->hdr.magic != EHF_MAGIC || hf->hdr.version != EHF_VERSION || hf->hdr.directory_size == 0u)
    {
        ehf_close((ExtHash)hf);
        return NULL;
    }

    hf->directory = (uint64_t *)malloc((size_t)hf->hdr.directory_size * sizeof(uint64_t));
    if (hf->directory == NULL)
    {
        ehf_close((ExtHash)hf);
        return NULL;
    }

    if (fread(hf->directory, sizeof(uint64_t), hf->hdr.directory_size, hf->fp) != hf->hdr.directory_size)
    {
        ehf_close((ExtHash)hf);
        return NULL;
    }

    hf->value_size_max_seen = 1u;
    return (ExtHash)hf;
}

void ehf_close(ExtHash h)
{
    ExtensibleHashFile *hf = (ExtensibleHashFile *)h;

    if (hf == NULL)
    {
        return;
    }

    if (hf->fp != NULL)
    {
        (void)ehf_flush_metadata(hf);
        (void)ehf_write_hfd(hf, "close");
        fclose(hf->fp);
    }

    free(hf->directory);
    free(hf->filename);
    free(hf);
}

int ehf_insert(ExtHash h, const char *key, const void *value, size_t value_size)
{
    ExtensibleHashFile *hf = (ExtensibleHashFile *)h;
    uint32_t index;
    uint64_t bucket_off;
    int rc;

    if (!ehf_validate_handle(hf) || !ehf_validate_key(key) || (value == NULL && value_size > 0u) || value_size > UINT16_MAX)
    {
        return -1;
    }

    ehf_update_value_size_max_seen(hf, value_size == 0u ? 1u : value_size);

    while (1)
    {
        index = ehf_directory_index(hf, key);
        bucket_off = hf->directory[index];
        rc = ehf_bucket_insert_raw(hf, bucket_off, key, value, value_size);
        if (rc == 1 || rc == 0 || rc == -1)
        {
            if (rc == 1)
            {
                (void)ehf_write_hfd(hf, NULL);
            }
            return rc;
        }

        if (!ehf_split_bucket(hf, index, key))
        {
            return -1;
        }
    }
}

int ehf_search(ExtHash h, const char *key, void *value_out, size_t *value_size)
{
    ExtensibleHashFile *hf = (ExtensibleHashFile *)h;
    uint32_t index;
    uint64_t bucket_off;
    uint32_t found_slot;
    uint32_t free_slot;
    int exists;
    EHFRecordHeader rh;

    if (!ehf_validate_handle(hf) || !ehf_validate_key(key) || value_size == NULL)
    {
        return -1;
    }

    index = ehf_directory_index(hf, key);
    bucket_off = hf->directory[index];

    if (!ehf_find_slot(hf, bucket_off, key, &found_slot, &exists, &free_slot))
    {
        return -1;
    }

    if (!exists)
    {
        return 0;
    }

    if (!ehf_read_record(hf, bucket_off, found_slot, &rh, value_out))
    {
        return -1;
    }

    *value_size = rh.value_size;
    return 1;
}

int ehf_remove(ExtHash h, const char *key)
{
    ExtensibleHashFile *hf = (ExtensibleHashFile *)h;
    uint32_t index;
    uint64_t bucket_off;
    uint32_t found_slot;
    uint32_t free_slot;
    int exists;
    EHFBucketHeader bh;

    if (!ehf_validate_handle(hf) || !ehf_validate_key(key))
    {
        return -1;
    }

    index = ehf_directory_index(hf, key);
    bucket_off = hf->directory[index];

    if (!ehf_find_slot(hf, bucket_off, key, &found_slot, &exists, &free_slot))
    {
        return -1;
    }
    if (!exists)
    {
        return 0;
    }

    if (!ehf_read_bucket_header(hf, bucket_off, &bh))
    {
        return -1;
    }

    if (!ehf_clear_slot(hf, bucket_off, found_slot))
    {
        return -1;
    }

    if (bh.record_count > 0u)
    {
        bh.record_count--;
    }
    if (!ehf_write_bucket_header(hf, bucket_off, &bh))
    {
        return -1;
    }

    (void)ehf_write_hfd(hf, NULL);
    return 1;
}

int ehf_exists(ExtHash h, const char *key)
{
    ExtensibleHashFile *hf = (ExtensibleHashFile *)h;
    uint32_t index;
    uint64_t bucket_off;
    uint32_t found_slot;
    uint32_t free_slot;
    int exists;

    if (!ehf_validate_handle(hf) || !ehf_validate_key(key))
    {
        return -1;
    }

    index = ehf_directory_index(hf, key);
    bucket_off = hf->directory[index];

    if (!ehf_find_slot(hf, bucket_off, key, &found_slot, &exists, &free_slot))
    {
        return -1;
    }

    return exists ? 1 : 0;
}

int ehf_global_depth(ExtHash h)
{
    ExtensibleHashFile *hf = (ExtensibleHashFile *)h;

    if (!ehf_validate_handle(hf))
    {
        return -1;
    }

    return (int)hf->hdr.global_depth;
}

int ehf_bucket_count(ExtHash h)
{
    ExtensibleHashFile *hf = (ExtensibleHashFile *)h;

    if (!ehf_validate_handle(hf))
    {
        return -1;
    }

    return (int)hf->hdr.bucket_count;
}
