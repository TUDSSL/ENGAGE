#include <stdint.h>

#include "nvm.h"

/*
 * We require the compiler to optimize this otherwise we waist quite a lot of
 * .text (flash)
 */
#pragma GCC push_options
#pragma GCC optimize "-O2"

#include "code_id.h"
#include "compile_time.h"

code_id_t code_id_is_new(void) {
  if (code_id != UNIX_TIMESTAMP) {
    return UNIX_TIMESTAMP;
  }
  return 0;
}

code_id_t code_id_update(void) {
    code_id = UNIX_TIMESTAMP;
    return code_id;
}

#pragma GCC pop_options
