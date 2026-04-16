#ifndef HASH_EXTENSIBLE_H
#define HASH_EXTENSIBLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void *HashExtFile;

    typedef enum
    {
        HEF_OK = 0,
        HEF_ERR_INVALID_ARG = -1,
        HEF_ERR_IO = -2,
        HEF_ERR_NOT_FOUND = -3,
        HEF_ERR_DUPLICATE_KEY = -4,
        HEF_ERR_CORRUPTED = -5,
        HEF_ERR_NO_MEMORY = -6
    } HashExtStatus;

    const char *hef_status_str(HashExtStatus status);

    HashExtStatus hef_create(const char *path,
                             uint32_t bucket_capacity,
                             uint32_t value_size,
                             uint32_t initial_global_depth,
                             HashExtFile *out_hf);

    HashExtStatus hef_open(const char *path, HashExtFile *out_hf);
    HashExtStatus hef_close(HashExtFile *io_hf);
    HashExtStatus hef_flush(HashExtFile hf);

    HashExtStatus hef_insert(HashExtFile hf, const char *key, const void *value);
    HashExtStatus hef_get(HashExtFile hf, const char *key, void *out_value);
    HashExtStatus hef_update(HashExtFile hf, const char *key, const void *value);
    HashExtStatus hef_remove(HashExtFile hf, const char *key, void *out_removed_value);
    HashExtStatus hef_contains(HashExtFile hf, const char *key, bool *out_contains);

    HashExtStatus hef_size(HashExtFile hf, uint32_t *out_size);
    HashExtStatus hef_value_size(HashExtFile hf, uint32_t *out_value_size);
    HashExtStatus hef_global_depth(HashExtFile hf, uint32_t *out_depth);
    HashExtStatus hef_bucket_count(HashExtFile hf, uint32_t *out_count);
    HashExtStatus hef_directory_entry_count(HashExtFile hf, uint32_t *out_count);

#ifdef __cplusplus
}
#endif

#endif
