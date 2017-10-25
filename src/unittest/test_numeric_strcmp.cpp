#include "test_numeric_strcmp.hpp"

#include "simgrid/msg.h"

#include "../machines.hpp"

int test_wrapper_sign_or_null(int n)
{
    if (n < 0)
        return -1;
    else if (n > 0)
        return 1;
    else
        return 0;
}

void test_wrapper_strcmp(const std::string & s1, const std::string & s2)
{
    int ret_my_cmp = string_numeric_comparator(s1, s2);
    int ret_strcmp = strcmp(s1.c_str(), s2.c_str());

    int sign_my_cmp = test_wrapper_sign_or_null(ret_my_cmp);
    int sign_strcmp = test_wrapper_sign_or_null(ret_strcmp);

    xbt_assert(sign_my_cmp == sign_strcmp,
               "string_numeric_compator and strcmp diverge with s1='%s' and s2='%s' "
               "(results are %d and %d)",
               s1.c_str(), s2.c_str(),
               ret_my_cmp, ret_strcmp);
}

void test_wrapper_my_strcmp(const std::string & s1, const std::string & s2, int expected_sign)
{
    xbt_assert(expected_sign == 0 ||
               expected_sign == 1 ||
               expected_sign == -1);

    int ret_my_cmp = string_numeric_comparator(s1, s2);
    int sign_my_cmp = test_wrapper_sign_or_null(ret_my_cmp);

    xbt_assert(sign_my_cmp == expected_sign,
               "string_numeric_compator returned an unexpected value with s1='%s' and s2='%s' "
               "(expected sign=%d, got value=%d)",
               s1.c_str(), s2.c_str(),
               expected_sign, ret_my_cmp);
}

void test_numeric_strcmp()
{
    // Classical lexicographical order
    test_wrapper_strcmp("a", "a");
    test_wrapper_strcmp("abcd", "abcd");

    test_wrapper_strcmp("a", "b");
    test_wrapper_strcmp("bouh", "bwah");
    test_wrapper_strcmp("me", "meh");

    test_wrapper_strcmp("b", "a");
    test_wrapper_strcmp("bwah", "bouh");
    test_wrapper_strcmp("meh", "me");

    // Numeric sort
    test_wrapper_strcmp("1", "1");
    test_wrapper_strcmp("1", "2");
    test_wrapper_strcmp("2", "1");

    test_wrapper_my_strcmp("1", "1", 0);
    test_wrapper_my_strcmp("1", "2", -1);
    test_wrapper_my_strcmp("2", "1", 1);

    test_wrapper_my_strcmp("machine9", "machine10", -1);
    test_wrapper_my_strcmp("machine10", "machine9", 1);

    test_wrapper_my_strcmp("machine09", "machine10", -1);
    test_wrapper_my_strcmp("machine10", "machine09", 1);

    test_wrapper_my_strcmp("machine09", "machine9", 0);
    test_wrapper_my_strcmp("machine009", "machine9", 0);
    test_wrapper_my_strcmp("machine009", "machine09", 0);

    test_wrapper_my_strcmp("qb0_qr0_qmobo1", "qb0_qr0_qmobo2", -1);
    test_wrapper_my_strcmp("qb0_qr0_qmobo2", "qb0_qr0_qmobo1", 1);

    test_wrapper_my_strcmp("qb0_qr1_qmobo2", "qb0_qr1_qmobo2", 0);
    test_wrapper_my_strcmp("qb00_qr1_qmobo2", "qb0_qr1_qmobo2", 0);
    test_wrapper_my_strcmp("qb0_qr01_qmobo2", "qb0_qr1_qmobo2", 0);
    test_wrapper_my_strcmp("qb0_qr1_qmobo02", "qb0_qr1_qmobo2", 0);
    test_wrapper_my_strcmp("qb0_qr01_qmobo02", "qb0_qr1_qmobo2", 0);
    test_wrapper_my_strcmp("qb00_qr1_qmobo02", "qb0_qr1_qmobo2", 0);
    test_wrapper_my_strcmp("qb00_qr01_qmobo2", "qb0_qr1_qmobo2", 0);
    test_wrapper_my_strcmp("qb00_qr01_qmobo02", "qb0_qr1_qmobo2", 0);
}
