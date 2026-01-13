#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <stddef.h>

ZTEST(he_app_tests, test_basic_assert_true)
{
    zassert_true(true, "This should always pass");
}

ZTEST_SUITE(he_app_tests, NULL, NULL, NULL, NULL, NULL);
