#ifndef PM_H
#define PM_H

#include "extensible_hash_file.h"

#include <stddef.h>

#define PM_OK 0
#define PM_ERR_INVALID_ARG -1
#define PM_ERR_IO -2
#define PM_ERR_PARSE -3
#define PM_ERR_HASH -4
#define PM_ERR_NOT_FOUND -5

size_t pm_habitante_record_size(void);

int processar_pm(const char *pm_path,
                 HashExtFile hf_habitantes,
                 HashExtFile hf_quadras);

#endif