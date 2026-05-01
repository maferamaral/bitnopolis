#include "../include/geo.h"
#include "../include/extensible_hash_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define GEO_MAX_LINE 512
#define GEO_CEP_LEN 64
#define GEO_COLOR_LEN 32

typedef struct
{
    char cep[GEO_CEP_LEN];
    double x;
    double y;
    double w;
    double h;
    double sw;
    char cfill[GEO_COLOR_LEN];
    char cstrk[GEO_COLOR_LEN];
    int removida;
} GeoQuadraRecord;

static void geo_trim_newline(char *s)
{
    size_t n;

    if (s == NULL)
        return;

    n = strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
    {
        s[n - 1] = '\0';
        n--;
    }
}

static char *geo_skip_spaces(char *s)
{
    while (s != NULL && *s != '\0' && isspace((unsigned char)*s))
        s++;

    return s;
}

static int geo_is_blank_or_comment(const char *s)
{
    const unsigned char *p = (const unsigned char *)s;

    while (*p != '\0' && isspace(*p))
        p++;

    return *p == '\0' || *p == '#';
}

static void geo_copy_string(char *dest, size_t dest_size, const char *src)
{
    if (dest == NULL || dest_size == 0)
        return;

    if (src == NULL)
    {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

static int geo_svg_begin(FILE *svg)
{
    if (svg == NULL)
        return GEO_ERR_INVALID_ARG;

    if (fprintf(svg,
                "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">\n") < 0)
        return GEO_ERR_IO;

    return GEO_OK;
}

static int geo_svg_end(FILE *svg)
{
    if (svg == NULL)
        return GEO_ERR_INVALID_ARG;

    if (fprintf(svg, "</svg>\n") < 0)
        return GEO_ERR_IO;

    return GEO_OK;
}

static int geo_svg_draw_quadra(FILE *svg, const GeoQuadraRecord *q)
{
    double tx;
    double ty;

    if (svg == NULL || q == NULL)
        return GEO_ERR_INVALID_ARG;

    tx = q->x + 3.0;
    ty = q->y + 12.0;

    if (fprintf(svg,
                "  <rect x=\"%.2lf\" y=\"%.2lf\" width=\"%.2lf\" height=\"%.2lf\" "
                "fill=\"%s\" stroke=\"%s\" stroke-width=\"%.2lf\" fill-opacity=\"0.5\" />\n",
                q->x, q->y, q->w, q->h, q->cfill, q->cstrk, q->sw) < 0)
        return GEO_ERR_IO;

    if (fprintf(svg,
                "  <text x=\"%.2lf\" y=\"%.2lf\" font-size=\"8\" fill=\"black\">%s</text>\n",
                tx, ty, q->cep) < 0)
        return GEO_ERR_IO;

    return GEO_OK;
}

size_t geo_quadra_record_size(void)
{
    return sizeof(GeoQuadraRecord);
}

int geo_processar_arquivo(const char *geo_path, HashExtFile hf_quadras, const char *svg_path)
{
    FILE *geo;
    FILE *svg = NULL;
    char line[GEO_MAX_LINE];

    double current_sw = 1.0;
    char current_cfill[GEO_COLOR_LEN] = "lightgray";
    char current_cstrk[GEO_COLOR_LEN] = "black";

    int status = GEO_OK;
    int line_number = 0;

    if (geo_path == NULL || hf_quadras == NULL)
        return GEO_ERR_INVALID_ARG;

    geo = fopen(geo_path, "r");
    if (geo == NULL)
        return GEO_ERR_IO;

    if (svg_path != NULL && svg_path[0] != '\0')
    {
        svg = fopen(svg_path, "w");
        if (svg == NULL)
        {
            fclose(geo);
            return GEO_ERR_IO;
        }

        status = geo_svg_begin(svg);
        if (status != GEO_OK)
        {
            fclose(svg);
            fclose(geo);
            return status;
        }
    }

    while (fgets(line, sizeof(line), geo) != NULL)
    {
        char comando[16];
        char *p;

        line_number++;
        geo_trim_newline(line);
        p = geo_skip_spaces(line);

        if (geo_is_blank_or_comment(p))
            continue;

        if (sscanf(p, "%15s", comando) != 1)
            continue;

        if (strcmp(comando, "cq") == 0)
        {
            double sw;
            char cfill[GEO_COLOR_LEN];
            char cstrk[GEO_COLOR_LEN];

            if (sscanf(p, "%*s %lf %31s %31s", &sw, cfill, cstrk) != 3)
            {
                fprintf(stderr, "Erro no .geo linha %d: comando cq invalido\n", line_number);
                status = GEO_ERR_PARSE;
                break;
            }

            current_sw = sw;
            geo_copy_string(current_cfill, sizeof(current_cfill), cfill);
            geo_copy_string(current_cstrk, sizeof(current_cstrk), cstrk);
        }
        else if (strcmp(comando, "q") == 0)
        {
            GeoQuadraRecord q;
            HashExtStatus hst;

            memset(&q, 0, sizeof(q));

            if (sscanf(p, "%*s %63s %lf %lf %lf %lf",
                       q.cep, &q.x, &q.y, &q.w, &q.h) != 5)
            {
                fprintf(stderr, "Erro no .geo linha %d: comando q invalido\n", line_number);
                status = GEO_ERR_PARSE;
                break;
            }

            q.sw = current_sw;
            q.removida = 0;
            geo_copy_string(q.cfill, sizeof(q.cfill), current_cfill);
            geo_copy_string(q.cstrk, sizeof(q.cstrk), current_cstrk);

            hst = hef_insert(hf_quadras, q.cep, &q);

            if (hst == HEF_ERR_DUPLICATE_KEY)
            {
                hst = hef_update(hf_quadras, q.cep, &q);
            }

            if (hst != HEF_OK)
            {
                fprintf(stderr,
                        "Erro no .geo linha %d: falha ao gravar quadra %s no hashfile (%s)\n",
                        line_number, q.cep, hef_status_str(hst));
                status = GEO_ERR_HASH;
                break;
            }

            if (svg != NULL)
            {
                status = geo_svg_draw_quadra(svg, &q);
                if (status != GEO_OK)
                    break;
            }
        }
        else
        {
            fprintf(stderr, "Aviso no .geo linha %d: comando ignorado: %s\n", line_number, comando);
        }
    }

    if (svg != NULL)
    {
        if (status == GEO_OK)
            status = geo_svg_end(svg);

        if (fclose(svg) != 0 && status == GEO_OK)
            status = GEO_ERR_IO;
    }

    if (fclose(geo) != 0 && status == GEO_OK)
        status = GEO_ERR_IO;

    return status;
}

int geo_buscar_quadra(HashExtFile hf_quadras,
                      const char *cep,
                      char *out_cep,
                      size_t out_cep_size,
                      double *out_x,
                      double *out_y,
                      double *out_w,
                      double *out_h,
                      double *out_sw,
                      char *out_cfill,
                      size_t out_cfill_size,
                      char *out_cstrk,
                      size_t out_cstrk_size)
{
    GeoQuadraRecord q;
    HashExtStatus st;

    if (hf_quadras == NULL || cep == NULL)
        return GEO_ERR_INVALID_ARG;

    st = hef_get(hf_quadras, cep, &q);
    if (st == HEF_ERR_NOT_FOUND)
        return GEO_ERR_NOT_FOUND;

    if (st != HEF_OK)
        return GEO_ERR_HASH;

    if (out_cep != NULL)
        geo_copy_string(out_cep, out_cep_size, q.cep);

    if (out_x != NULL)
        *out_x = q.x;

    if (out_y != NULL)
        *out_y = q.y;

    if (out_w != NULL)
        *out_w = q.w;

    if (out_h != NULL)
        *out_h = q.h;

    if (out_sw != NULL)
        *out_sw = q.sw;

    if (out_cfill != NULL)
        geo_copy_string(out_cfill, out_cfill_size, q.cfill);

    if (out_cstrk != NULL)
        geo_copy_string(out_cstrk, out_cstrk_size, q.cstrk);

    return GEO_OK;
}