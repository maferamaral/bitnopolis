#ifndef EXTENSIBLE_HASH_FILE_H
#define EXTENSIBLE_HASH_FILE_H

#include <stddef.h>

/*
 * Tipo opaco para a tabela hash extensível.
 */
typedef void *ExtHash;

/*
 * Cria um novo arquivo de hash extensível.
 * bucket_size: número máximo de registros por bucket
 */
ExtHash ehf_create(const char *filename, size_t bucket_size);

/*
 * Abre um arquivo de hash existente.
 */
ExtHash ehf_open(const char *filename);

/*
 * Fecha a estrutura e libera memória.
 */
void ehf_close(ExtHash h);

/*
 * Insere um par chave-valor.
 * Retorna 1 se inseriu, 0 se chave já existe, -1 erro.
 */
int ehf_insert(ExtHash h, const char *key, const void *value, size_t value_size);

/*
 * Busca um valor pela chave.
 * Retorna 1 se encontrou, 0 se não, -1 erro.
 */
int ehf_search(ExtHash h, const char *key, void *value_out, size_t *value_size);

/*
 * Remove uma chave.
 * Retorna 1 se removeu, 0 se não existe, -1 erro.
 */
int ehf_remove(ExtHash h, const char *key);

/*
 * Verifica se uma chave existe.
 */
int ehf_exists(ExtHash h, const char *key);

/*
 * Retorna a profundidade global (para testes).
 */
int ehf_global_depth(ExtHash h);

/*
 * Retorna quantidade de buckets (para testes).
 */
int ehf_bucket_count(ExtHash h);

#endif