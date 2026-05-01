#include "../unity/unity.h"
#include "../include/geo.h"
#include "../include/pm.h"
#include "../include/extensible_hash_file.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PM_TEST_GEO "tst_pm_geo.geo"
#define PM_TEST_FILE "tst_pm_input.pm"
#define PM_TEST_QUADRAS "tst_pm_quadras.hf"
#define PM_TEST_HABITANTES "tst_pm_habitantes.hf"

typedef struct
{
    char cpf[32];
    char nome[64];
    char sobrenome[64];
    char sexo;
    char nasc[16];
    int tem_endereco;
    char cep[64];
    char face;
    int num;
    char compl[64];
    int vivo;
} PmRecordTeste;

static HashExtFile g_quadras = NULL;
static HashExtFile g_habitantes = NULL;

static void write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_NOT_EQUAL(EOF, fputs(content, f));
    TEST_ASSERT_EQUAL_INT(0, fclose(f));
}

void setUp(void)
{
    remove(PM_TEST_GEO);
    remove(PM_TEST_FILE);
    remove(PM_TEST_QUADRAS);
    remove(PM_TEST_HABITANTES);
    g_quadras = NULL;
    g_habitantes = NULL;
}

void tearDown(void)
{
    if (g_habitantes != NULL)
        (void)hef_close(&g_habitantes);
    if (g_quadras != NULL)
        (void)hef_close(&g_quadras);

    remove(PM_TEST_GEO);
    remove(PM_TEST_FILE);
    remove(PM_TEST_QUADRAS);
    remove(PM_TEST_HABITANTES);
}

static void criar_hashes_com_quadra(void)
{
    TEST_ASSERT_EQUAL_INT(HEF_OK,
                          hef_create(PM_TEST_QUADRAS,
                                     4u,
                                     (uint32_t)geo_quadra_record_size(),
                                     1u,
                                     &g_quadras));
    TEST_ASSERT_EQUAL_INT(HEF_OK,
                          hef_create(PM_TEST_HABITANTES,
                                     4u,
                                     (uint32_t)pm_habitante_record_size(),
                                     1u,
                                     &g_habitantes));

    write_text_file(PM_TEST_GEO,
                    "cq 1.0px Olive Moccasin\n"
                    "q b01.1 10 20 30 40\n");
    TEST_ASSERT_EQUAL_INT(GEO_OK, geo_processar_arquivo(PM_TEST_GEO, g_quadras, NULL));
}

void test_pm_deve_cadastrar_habitante_e_endereco(void)
{
    PmRecordTeste h;

    criar_hashes_com_quadra();
    write_text_file(PM_TEST_FILE,
                    "p 111 Ana Silva F 2000-01-01\n"
                    "m 111 b01.1 N 12 ap101\n");

    TEST_ASSERT_EQUAL_INT(PM_OK, processar_pm(PM_TEST_FILE, g_habitantes, g_quadras));
    TEST_ASSERT_EQUAL_INT(HEF_OK, hef_get(g_habitantes, "111", &h));

    TEST_ASSERT_EQUAL_STRING("Ana", h.nome);
    TEST_ASSERT_EQUAL_STRING("Silva", h.sobrenome);
    TEST_ASSERT_EQUAL_CHAR('F', h.sexo);
    TEST_ASSERT_EQUAL_STRING("2000-01-01", h.nasc);
    TEST_ASSERT_TRUE(h.vivo);
    TEST_ASSERT_TRUE(h.tem_endereco);
    TEST_ASSERT_EQUAL_STRING("b01.1", h.cep);
    TEST_ASSERT_EQUAL_CHAR('N', h.face);
    TEST_ASSERT_EQUAL_INT(12, h.num);
    TEST_ASSERT_EQUAL_STRING("ap101", h.compl);
}

void test_pm_deve_falhar_quando_morador_nao_foi_cadastrado(void)
{
    criar_hashes_com_quadra();
    write_text_file(PM_TEST_FILE, "m 999 b01.1 S 4 casa\n");

    TEST_ASSERT_EQUAL_INT(PM_ERR_NOT_FOUND, processar_pm(PM_TEST_FILE, g_habitantes, g_quadras));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pm_deve_cadastrar_habitante_e_endereco);
    RUN_TEST(test_pm_deve_falhar_quando_morador_nao_foi_cadastrado);
    return UNITY_END();
}
