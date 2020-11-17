#ifndef BLISS_ALLOCATOR_DEBUG_UTIL_H_
#define BLISS_ALLOCATOR_DEBUG_UTIL_H_

#include "bliss_allocator.h"

void bliss_dbg_print_block(bliss_block_t *block);
void bliss_dbg_print_blocks(size_t n);
void bliss_dbg_print_initialized_blocks(void);
void bliss_dbg_print_all_blocks(void);
void bliss_dbg_print_active_allocator(void);

#endif /* BLISS_ALLOCATOR_DEBUG_UTIL_H_ */
