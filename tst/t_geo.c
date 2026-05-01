#include "../unity/unity.h"
#include "../include/geo.h"
#include "../include/extensible_hash_file.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define GEO_TEST_FILE "tst_geo_input.geo"
#define GEO_TEST_SVG "tst_geo_output.svg"
#define GEO_TEST_HASH "tst_geo_quadras.hf"

static HashExtFile g_quadras = NULL;

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
    char buffer[4096];
    size_t n;
    int found = 0;

    TEST_ASSERT_NOT_NULL(f);
    n = fread(buffer, 1u, sizeof(buffer) - 1u, f);
    buffer[n] = '\0';
    found = strstr(buffer, needle) != NULL;
    TEST_ASSERT_EQUAL_INT(0, fclose(f));

    return found;
}

void setUp(void)
{
    remove(GEO_TEST_FILE);
    remove(GEO_TEST_SVG);
    remove(GEO_TEST_HASH);
    g_quadras = NULL;
}

void tearDown(void)
{
    if (g_quadras != NULL)
        (void)hef_close(&g_quadras);

    remove(GEO_TEST_FILE);
    remove(GEO_TEST_SVG);
    remove(GEO_TEST_HASH);
}

static void criar_hash_quadras(void)
{
    TEST_ASSERT_EQUAL_INT(HEF_OK,
                          hef_create(GEO_TEST_HASH,
                                     4u,
                                     (uint32_t)geo_quadra_record_size(),
                                     1u,
                                     &g_quadras));
    TEST_ASSERT_NOT_NULL(g_quadras);
}

void test_geo_deve_ler_quadra_e_gerar_svg_com_cores(void)
{
    char cep[64];
    double x, y, w, h, sw;
    char fill[32];
    char stroke[32];

    criar_hash_quadras();
    write_text_file(GEO_TEST_FILE,
                    "cq 1.0px Olive Moccasin\n"
                    "q b01.1 10 20 30 40\n");

    TEST_ASSERT_EQUAL_INT(GEO_OK, geo_processar_arquivo(GEO_TEST_FILE, g_quadras, GEO_TEST_SVG));
    TEST_ASSERT_EQUAL_INT(GEO_OK,
                          geo_buscar_quadra(g_quadras,
                                            "b01.1",
                                            cep,
                                            sizeof(cep),
                                            &x,
                                            &y,
                                            &w,
                                            &h,
                                            &sw,
                                            fill,
                                            sizeof(fill),
                                            stroke,
                                            sizeof(stroke)));

    TEST_ASSERT_EQUAL_STRING("b01.1", cep);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 10.0, x);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 20.0, y);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 30.0, w);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 40.0, h);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 1.0, sw);
    TEST_ASSERT_EQUAL_STRING("Olive", fill);
    TEST_ASSERT_EQUAL_STRING("Moccasin", stroke);
    TEST_ASSERT_TRUE(file_contains(GEO_TEST_SVG, "viewBox="));
    TEST_ASSERT_TRUE(file_contains(GEO_TEST_SVG, "fill=\"Olive\" stroke=\"Moccasin\""));
}

void test_geo_deve_retornar_not_found_para_quadra_inexistente(void)
{
    criar_hash_quadras();

    TEST_ASSERT_EQUAL_INT(GEO_ERR_NOT_FOUND,
                          geo_buscar_quadra(g_quadras,
                                            "nao-existe",
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_geo_deve_ler_quadra_e_gerar_svg_com_cores);
    RUN_TEST(test_geo_deve_retornar_not_found_para_quadra_inexistente);
    return UNITY_END();
}
