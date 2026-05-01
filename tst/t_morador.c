#include "../unity/unity.h"
#include "../include/morador.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_deve_criar_morador_com_dados_basicos(void)
{
    Morador m = morador_create("123", "Ana", "Silva", 'F', "2000-01-02");

    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_STRING("123", morador_get_cpf(m));
    TEST_ASSERT_EQUAL_STRING("Ana", morador_get_nome(m));
    TEST_ASSERT_EQUAL_STRING("Silva", morador_get_sobrenome(m));
    TEST_ASSERT_EQUAL_CHAR('F', morador_get_sexo(m));
    TEST_ASSERT_EQUAL_STRING("2000-01-02", morador_get_nasc(m));
    TEST_ASSERT_FALSE(morador_tem_endereco(m));

    morador_destroy(m);
}

void test_deve_definir_e_remover_endereco(void)
{
    Morador m = morador_create("456", "Bruno", "Costa", 'M', "1999-03-04");

    TEST_ASSERT_NOT_NULL(m);
    morador_set_endereco(m, "b01.1", 'N', 25, "ap101");

    TEST_ASSERT_TRUE(morador_tem_endereco(m));
    TEST_ASSERT_EQUAL_STRING("b01.1", morador_get_cep(m));
    TEST_ASSERT_EQUAL_CHAR('N', morador_get_face(m));
    TEST_ASSERT_EQUAL_INT(25, morador_get_num(m));
    TEST_ASSERT_EQUAL_STRING("ap101", morador_get_compl(m));

    morador_remove_endereco(m);
    TEST_ASSERT_FALSE(morador_tem_endereco(m));

    morador_destroy(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_deve_criar_morador_com_dados_basicos);
    RUN_TEST(test_deve_definir_e_remover_endereco);
    return UNITY_END();
}
