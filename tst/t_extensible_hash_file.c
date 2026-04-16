#include "../unity/unity.h"
#include "../include/extensible_hash_file.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_HASH_FILE_PATH "./hef_test_data.bin"
#define TEST_BUCKET_CAPACITY 2u
#define TEST_INITIAL_GLOBAL_DEPTH 1u

#define TEST_ASSERT_STATUS_OK(expr)                                              \
    do                                                                           \
    {                                                                            \
        HashExtStatus _status = (expr);                                          \
        TEST_ASSERT_EQUAL_INT_MESSAGE(HEF_OK, _status, hef_status_str(_status)); \
    } while (0)

typedef struct
{
    int id;
    char nome[32];
    double saldo;
} RegistroTeste;

static HashExtFile g_hf = NULL;

static RegistroTeste make_registro(int id, const char *nome, double saldo)
{
    RegistroTeste r;
    r.id = id;
    memset(r.nome, 0, sizeof(r.nome));
    if (nome != NULL)
    {
        strncpy(r.nome, nome, sizeof(r.nome) - 1u);
    }
    r.saldo = saldo;
    return r;
}

void setUp(void)
{
    remove(TEST_HASH_FILE_PATH);
    g_hf = NULL;
}

void tearDown(void)
{
    if (g_hf != NULL)
    {
        (void)hef_close(&g_hf);
    }
    remove(TEST_HASH_FILE_PATH);
}

static void create_empty_test_file(void)
{
    TEST_ASSERT_STATUS_OK(hef_create(TEST_HASH_FILE_PATH,
                                     TEST_BUCKET_CAPACITY,
                                     (uint32_t)sizeof(RegistroTeste),
                                     TEST_INITIAL_GLOBAL_DEPTH,
                                     &g_hf));
    TEST_ASSERT_NOT_NULL(g_hf);
}

static void reopen_test_file(void)
{
    TEST_ASSERT_STATUS_OK(hef_open(TEST_HASH_FILE_PATH, &g_hf));
    TEST_ASSERT_NOT_NULL(g_hf);
}

static void close_if_open(void)
{
    if (g_hf != NULL)
    {
        TEST_ASSERT_STATUS_OK(hef_close(&g_hf));
        TEST_ASSERT_NULL(g_hf);
    }
}

void test_deve_criar_arquivo_e_obter_dados_iniciais(void)
{
    uint32_t size = 99u;
    uint32_t value_size = 0u;
    uint32_t global_depth = 0u;
    uint32_t bucket_count = 0u;
    uint32_t directory_entry_count = 0u;

    create_empty_test_file();
    TEST_ASSERT_STATUS_OK(hef_size(g_hf, &size));
    TEST_ASSERT_STATUS_OK(hef_value_size(g_hf, &value_size));
    TEST_ASSERT_STATUS_OK(hef_global_depth(g_hf, &global_depth));
    TEST_ASSERT_STATUS_OK(hef_bucket_count(g_hf, &bucket_count));
    TEST_ASSERT_STATUS_OK(hef_directory_entry_count(g_hf, &directory_entry_count));

    TEST_ASSERT_EQUAL_UINT32(0u, size);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(RegistroTeste), value_size);
    TEST_ASSERT_TRUE(global_depth >= 1u);
    TEST_ASSERT_TRUE(bucket_count >= 2u);
    TEST_ASSERT_TRUE(directory_entry_count >= 2u);
}

void test_deve_inserir_e_buscar_registro_por_chave(void)
{
    RegistroTeste inserido = make_registro(10, "ana", 125.50);
    RegistroTeste lido;

    create_empty_test_file();
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "cli-10", &inserido));
    TEST_ASSERT_STATUS_OK(hef_get(g_hf, "cli-10", &lido));

    TEST_ASSERT_EQUAL_INT(inserido.id, lido.id);
    TEST_ASSERT_EQUAL_STRING(inserido.nome, lido.nome);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, inserido.saldo, lido.saldo);
}

void test_nao_deve_aceitar_chave_duplicada_em_insert(void)
{
    RegistroTeste a = make_registro(1, "primeiro", 10.0);
    RegistroTeste b = make_registro(2, "segundo", 20.0);

    create_empty_test_file();
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "dup", &a));
    TEST_ASSERT_EQUAL_INT(HEF_ERR_DUPLICATE_KEY, hef_insert(g_hf, "dup", &b));
}

void test_deve_indicar_presenca_ou_ausencia_de_chave(void)
{
    RegistroTeste r = make_registro(7, "bia", 33.0);
    bool contains = false;

    create_empty_test_file();
    TEST_ASSERT_STATUS_OK(hef_contains(g_hf, "c7", &contains));
    TEST_ASSERT_FALSE(contains);

    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "c7", &r));
    TEST_ASSERT_STATUS_OK(hef_contains(g_hf, "c7", &contains));
    TEST_ASSERT_TRUE(contains);
}

void test_deve_atualizar_valor_de_chave_existente(void)
{
    RegistroTeste original = make_registro(9, "caio", 40.0);
    RegistroTeste atualizado = make_registro(9, "caio", 99.9);
    RegistroTeste lido;

    create_empty_test_file();
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "cli-9", &original));
    TEST_ASSERT_STATUS_OK(hef_update(g_hf, "cli-9", &atualizado));
    TEST_ASSERT_STATUS_OK(hef_get(g_hf, "cli-9", &lido));

    TEST_ASSERT_EQUAL_INT(9, lido.id);
    TEST_ASSERT_EQUAL_STRING("caio", lido.nome);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 99.9, lido.saldo);
}

void test_remove_deve_excluir_chave_e_retornar_valor_removido(void)
{
    RegistroTeste inserido = make_registro(13, "dora", 11.25);
    RegistroTeste removido;
    bool contains = true;

    create_empty_test_file();
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "cli-13", &inserido));
    TEST_ASSERT_STATUS_OK(hef_remove(g_hf, "cli-13", &removido));

    TEST_ASSERT_EQUAL_INT(inserido.id, removido.id);
    TEST_ASSERT_EQUAL_STRING(inserido.nome, removido.nome);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, inserido.saldo, removido.saldo);

    TEST_ASSERT_STATUS_OK(hef_contains(g_hf, "cli-13", &contains));
    TEST_ASSERT_FALSE(contains);
}

void test_remove_deve_falhar_quando_chave_nao_existe(void)
{
    RegistroTeste removido;

    create_empty_test_file();
    TEST_ASSERT_EQUAL_INT(HEF_ERR_NOT_FOUND, hef_remove(g_hf, "inexistente", &removido));
}

void test_dados_devem_persistir_apos_fechar_e_reabrir_arquivo(void)
{
    RegistroTeste inserido = make_registro(21, "eva", 501.0);
    RegistroTeste lido;

    create_empty_test_file();
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "cli-21", &inserido));
    TEST_ASSERT_STATUS_OK(hef_flush(g_hf));
    close_if_open();

    reopen_test_file();
    TEST_ASSERT_STATUS_OK(hef_get(g_hf, "cli-21", &lido));

    TEST_ASSERT_EQUAL_INT(21, lido.id);
    TEST_ASSERT_EQUAL_STRING("eva", lido.nome);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 501.0, lido.saldo);
}

void test_varias_insercoes_devem_aumentar_quantidade_de_registros(void)
{
    uint32_t size = 0u;

    create_empty_test_file();
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "k1", &(RegistroTeste){1, "a", 1.0}));
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "k2", &(RegistroTeste){2, "b", 2.0}));
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "k3", &(RegistroTeste){3, "c", 3.0}));
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "k4", &(RegistroTeste){4, "d", 4.0}));
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "k5", &(RegistroTeste){5, "e", 5.0}));

    TEST_ASSERT_STATUS_OK(hef_size(g_hf, &size));
    TEST_ASSERT_EQUAL_UINT32(5u, size);
}

void test_insercoes_suficientes_devem_provocar_split_ou_expansao_de_diretorio(void)
{
    uint32_t bucket_count_before = 0u;
    uint32_t global_depth_before = 0u;
    uint32_t directory_count_before = 0u;
    uint32_t bucket_count_after = 0u;
    uint32_t global_depth_after = 0u;
    uint32_t directory_count_after = 0u;

    create_empty_test_file();
    TEST_ASSERT_STATUS_OK(hef_bucket_count(g_hf, &bucket_count_before));
    TEST_ASSERT_STATUS_OK(hef_global_depth(g_hf, &global_depth_before));
    TEST_ASSERT_STATUS_OK(hef_directory_entry_count(g_hf, &directory_count_before));

    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "a1", &(RegistroTeste){1, "a1", 1.0}));
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "a2", &(RegistroTeste){2, "a2", 2.0}));
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "a3", &(RegistroTeste){3, "a3", 3.0}));
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "a4", &(RegistroTeste){4, "a4", 4.0}));
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "a5", &(RegistroTeste){5, "a5", 5.0}));
    TEST_ASSERT_STATUS_OK(hef_insert(g_hf, "a6", &(RegistroTeste){6, "a6", 6.0}));

    TEST_ASSERT_STATUS_OK(hef_bucket_count(g_hf, &bucket_count_after));
    TEST_ASSERT_STATUS_OK(hef_global_depth(g_hf, &global_depth_after));
    TEST_ASSERT_STATUS_OK(hef_directory_entry_count(g_hf, &directory_count_after));

    TEST_ASSERT_TRUE(bucket_count_after >= bucket_count_before);
    TEST_ASSERT_TRUE(global_depth_after >= global_depth_before);
    TEST_ASSERT_TRUE(directory_count_after >= directory_count_before);
    TEST_ASSERT_TRUE(bucket_count_after > bucket_count_before || global_depth_after > global_depth_before);
}

void test_abrir_arquivo_inexistente_deve_falhar(void)
{
    TEST_ASSERT_EQUAL_INT(HEF_ERR_IO, hef_open("arquivo_que_nao_existe.bin", &g_hf));
    TEST_ASSERT_NULL(g_hf);
}

void test_create_deve_validar_argumentos_basicos(void)
{
    TEST_ASSERT_EQUAL_INT(HEF_ERR_INVALID_ARG, hef_create(NULL, 2u, 8u, 1u, &g_hf));
    TEST_ASSERT_EQUAL_INT(HEF_ERR_INVALID_ARG, hef_create(TEST_HASH_FILE_PATH, 0u, 8u, 1u, &g_hf));
    TEST_ASSERT_EQUAL_INT(HEF_ERR_INVALID_ARG, hef_create(TEST_HASH_FILE_PATH, 2u, 0u, 1u, &g_hf));
    TEST_ASSERT_EQUAL_INT(HEF_ERR_INVALID_ARG, hef_open(NULL, &g_hf));
    TEST_ASSERT_EQUAL_INT(HEF_ERR_INVALID_ARG, hef_contains(NULL, "x", &(bool){false}));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_deve_criar_arquivo_e_obter_dados_iniciais);
    RUN_TEST(test_deve_inserir_e_buscar_registro_por_chave);
    RUN_TEST(test_nao_deve_aceitar_chave_duplicada_em_insert);
    RUN_TEST(test_deve_indicar_presenca_ou_ausencia_de_chave);
    RUN_TEST(test_deve_atualizar_valor_de_chave_existente);
    RUN_TEST(test_remove_deve_excluir_chave_e_retornar_valor_removido);
    RUN_TEST(test_remove_deve_falhar_quando_chave_nao_existe);
    RUN_TEST(test_dados_devem_persistir_apos_fechar_e_reabrir_arquivo);
    RUN_TEST(test_varias_insercoes_devem_aumentar_quantidade_de_registros);
    RUN_TEST(test_insercoes_suficientes_devem_provocar_split_ou_expansao_de_diretorio);
    RUN_TEST(test_abrir_arquivo_inexistente_deve_falhar);
    RUN_TEST(test_create_deve_validar_argumentos_basicos);
    return UNITY_END();
}
