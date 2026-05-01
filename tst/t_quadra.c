#include "../unity/unity.h"
#include "../include/quadra.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_deve_criar_quadra_com_geometria_e_estilo(void)
{
    Quadra q = quadra_create("b01.1", 10.0, 20.0, 30.0, 40.0, "Olive", "Moccasin", 2.5);

    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_STRING("b01.1", quadra_get_cep(q));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 10.0, quadra_get_x(q));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 20.0, quadra_get_y(q));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 30.0, quadra_get_w(q));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 40.0, quadra_get_h(q));
    TEST_ASSERT_EQUAL_STRING("Olive", quadra_get_fill(q));
    TEST_ASSERT_EQUAL_STRING("Moccasin", quadra_get_stroke(q));
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 2.5, quadra_get_sw(q));

    quadra_destroy(q);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_deve_criar_quadra_com_geometria_e_estilo);
    return UNITY_END();
}
