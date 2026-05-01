#ifndef GEO_H
#define GEO_H

#include "extensible_hash_file.h"

#include <stddef.h>
#include <stdio.h>

#define GEO_OK 0
#define GEO_ERR_INVALID_ARG -1
#define GEO_ERR_IO -2
#define GEO_ERR_PARSE -3
#define GEO_ERR_HASH -4
#define GEO_ERR_NOT_FOUND -5

size_t geo_quadra_record_size(void);

int geo_processar_arquivo(const char *geo_path,
                          HashExtFile hf_quadras,
                          const char *svg_path);

int geo_obter_limites_quadras(HashExtFile hf_quadras,
                              double *out_min_x,
                              double *out_min_y,
                              double *out_max_x,
                              double *out_max_y);

int geo_escrever_quadras_svg(FILE *svg, HashExtFile hf_quadras);

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
                      size_t out_cstrk_size);

#endif
