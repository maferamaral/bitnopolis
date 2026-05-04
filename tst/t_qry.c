#include "../unity/unity.h"
#include "../include/geo.h"
#include "../include/pm.h"
#include "../include/qry.h"
#include "../include/extensible_hash_file.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define QRY_TEST_GEO "tst_qry_geo.geo"
#define QRY_TEST_PM "tst_qry_pm.pm"
#define QRY_TEST_QRY "tst_qry_input.qry"
#define QRY_TEST_TXT "tst_qry_output.txt"
#define QRY_TEST_SVG "tst_qry_output.svg"
#define QRY_TEST_QUADRAS "tst_qry_quadras.hf"
#define QRY_TEST_HABITANTES "tst_qry_habitantes.hf"

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
} QryRecordTeste;

static HashExtFile g_quadras = NULL;
static HashExtFile g_habitantes = NULL;

static void write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_NOT_EQUAL(EOF, fputs(content, f));
    TEST_ASSERT_EQUAL_INT(0, fclose(f));
}

static int file_contains(const char *path, const char *needle)
{
    FILE *f = fopen(path, "r");
    char buffer[8192];
    size_t n;
    int found;

    TEST_ASSERT_NOT_NULL(f);
    n = fread(buffer, 1u, sizeof(buffer) - 1u, f);
    buffer[n] = '\0';
    found = strstr(buffer, needle) != NULL;
    TEST_ASSERT_EQUAL_INT(0, fclose(f));

    return found;
}

void setUp(void)
{
    remove(QRY_TEST_GEO);
    remove(QRY_TEST_PM);
    remove(QRY_TEST_QRY);
    remove(QRY_TEST_TXT);
    remove(QRY_TEST_SVG);
    remove(QRY_TEST_QUADRAS);
    remove(QRY_TEST_HABITANTES);
    g_quadras = NULL;
    g_habitantes = NULL;
}

void tearDown(void)
{
    if (g_habitantes != NULL)
        (void)hef_close(&g_habitantes);
    if (g_quadras != NULL)
        (void)hef_close(&g_quadras);

    remove(QRY_TEST_GEO);
    remove(QRY_TEST_PM);
    remove(QRY_TEST_QRY);
    remove(QRY_TEST_TXT);
    remove(QRY_TEST_SVG);
    remove(QRY_TEST_QUADRAS);
    remove(QRY_TEST_HABITANTES);
}

static void preparar_cidade_e_habitantes(void)
{
    TEST_ASSERT_EQUAL_INT(HEF_OK,
                          hef_create(QRY_TEST_QUADRAS,
                                     4u,
                                     (uint32_t)geo_quadra_record_size(),
                                     1u,
                                     &g_quadras));
    TEST_ASSERT_EQUAL_INT(HEF_OK,
                          hef_create(QRY_TEST_HABITANTES,
                                     4u,
                                     (uint32_t)pm_habitante_record_size(),
                                     1u,
                                     &g_habitantes));

    write_text_file(QRY_TEST_GEO,
                    "cq 1.0px Olive Moccasin\n"
                    "q b01.1 10 20 30 40\n"
                    "q b01.2 60 20 30 40\n");
    write_text_file(QRY_TEST_PM,
                    "p 111 Ana Silva F 2000-01-01\n"
                    "p 222 Beto Souza M 1999-02-03\n"
                    "m 111 b01.1 N 12 ap101\n"
                    "m 222 b01.2 S 8 casa\n");

    TEST_ASSERT_EQUAL_INT(GEO_OK, geo_processar_arquivo(QRY_TEST_GEO, g_quadras, NULL));
    TEST_ASSERT_EQUAL_INT(PM_OK, processar_pm(QRY_TEST_PM, g_habitantes, g_quadras));
}

void test_qry_h_deve_escrever_dados_do_habitante(void)
{
    preparar_cidade_e_habitantes();
    write_text_file(QRY_TEST_QRY, "h? 111\n");

    TEST_ASSERT_EQUAL_INT(QRY_OK,
                          processar_qry(QRY_TEST_QRY,
                                        g_quadras,
                                        g_habitantes,
                                        QRY_TEST_TXT,
                                        QRY_TEST_SVG));

    TEST_ASSERT_TRUE(file_contains(QRY_TEST_TXT, "cpf: 111"));
    TEST_ASSERT_TRUE(file_contains(QRY_TEST_TXT, "nome: Ana Silva"));
    TEST_ASSERT_TRUE(file_contains(QRY_TEST_SVG, "fill=\"Olive\" stroke=\"Moccasin\""));
}

void test_qry_rip_deve_marcar_obito_e_desenhar_cruz_vermelha(void)
{
    QryRecordTeste h;

    preparar_cidade_e_habitantes();
    write_text_file(QRY_TEST_QRY, "rip 111\n");

    TEST_ASSERT_EQUAL_INT(QRY_OK,
                          processar_qry(QRY_TEST_QRY,
                                        g_quadras,
                                        g_habitantes,
                                        QRY_TEST_TXT,
                                        QRY_TEST_SVG));
    TEST_ASSERT_EQUAL_INT(HEF_OK, hef_get(g_habitantes, "111", &h));
    TEST_ASSERT_FALSE(h.vivo);
    TEST_ASSERT_TRUE(file_contains(QRY_TEST_SVG, "stroke=\"red\""));
    TEST_ASSERT_TRUE(file_contains(QRY_TEST_SVG, "x1=\"17.00\" y1=\"60.00\" x2=\"27.00\" y2=\"60.00\""));
}

void test_qry_rq_deve_remover_quadra_do_svg_final(void)
{
    preparar_cidade_e_habitantes();
    write_text_file(QRY_TEST_QRY, "rq b01.1\n");

    TEST_ASSERT_EQUAL_INT(QRY_OK,
                          processar_qry(QRY_TEST_QRY,
                                        g_quadras,
                                        g_habitantes,
                                        QRY_TEST_TXT,
                                        QRY_TEST_SVG));

    TEST_ASSERT_FALSE(file_contains(QRY_TEST_SVG, ">b01.1<"));
    TEST_ASSERT_TRUE(file_contains(QRY_TEST_SVG, ">b01.2<"));
    TEST_ASSERT_EQUAL_INT(GEO_ERR_NOT_FOUND,
                          geo_buscar_quadra(g_quadras,
                                            "b01.1",
                                            NULL,
                                            0u,
                                            NULL,
                                            NULL,
                                            NULL,
                                            NULL,
                                            NULL,
                                            NULL,
                                            0u,
                                            NULL,
                                            0u));
}

void test_qry_deve_desenhar_leste_e_oeste_invertidos_no_svg(void)
{
    preparar_cidade_e_habitantes();
    write_text_file(QRY_TEST_QRY,
                    "mud 111 b01.1 L 10 ap102\n"
                    "mud 222 b01.2 O 10 casa\n");

    TEST_ASSERT_EQUAL_INT(QRY_OK,
                          processar_qry(QRY_TEST_QRY,
                                        g_quadras,
                                        g_habitantes,
                                        QRY_TEST_TXT,
                                        QRY_TEST_SVG));

    TEST_ASSERT_TRUE(file_contains(QRY_TEST_SVG, "x=\"5.00\" y=\"45.00\""));
    TEST_ASSERT_TRUE(file_contains(QRY_TEST_SVG, "x=\"85.00\" y=\"45.00\""));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_qry_h_deve_escrever_dados_do_habitante);
    RUN_TEST(test_qry_rip_deve_marcar_obito_e_desenhar_cruz_vermelha);
    RUN_TEST(test_qry_rq_deve_remover_quadra_do_svg_final);
    RUN_TEST(test_qry_deve_desenhar_leste_e_oeste_invertidos_no_svg);
    return UNITY_END();
}
