#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include "cmocka.h"

/**
 * For memory-leak testing
 * TODO has an error somewhere
 */
#if 0
#define malloc(size) test_malloc(size, __FILE__, __LINE__)
#define malloc(size) test_malloc(size, __FILE__, __LINE__)
#define realloc test_realloc
#define free test_free
#endif

#define test(t_)    static void t_(void **state)
//#define TSTEH(t_)   cmocka_unit_test(t_)

// macro for generating:
//  static void test_TESTNAME(void **state)
//#define test(t_)     TSTH(t_)

// macro for generating:
//  cmocka_unit_test(test_TESTNAME))
//#define test_entry(t_)    TSTEH(t_)
