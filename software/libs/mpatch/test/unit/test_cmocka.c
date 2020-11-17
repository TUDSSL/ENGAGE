#include "testcommon.h"

test(test_cmocka)
{
    assert_true(true == true);
}


/*
* Register Tests
*/
int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cmocka),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
