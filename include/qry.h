#ifndef QRY_H
#define QRY_H

#include "extensible_hash_file.h"

#define QRY_OK 0
#define QRY_ERR_INVALID_ARG -1
#define QRY_ERR_IO -2
#define QRY_ERR_PARSE -3
#define QRY_ERR_HASH -4
#define QRY_ERR_NOT_FOUND -5

int processar_qry(const char *qry_path,
                  HashExtFile hf_quadras,
                  HashExtFile hf_habitantes,
                  const char *txt_path,
                  const char *svg_path);

#endif