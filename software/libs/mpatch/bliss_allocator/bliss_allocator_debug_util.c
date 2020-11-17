#ifndef BLISS_ALLOCATOR_DEBUG_UTIL_H_
#define BLISS_ALLOCATOR_DEBUG_UTIL_H_

#include <stdio.h>

#include "bliss_allocator.h"
#include "bliss_allocator_nvm.h"
#include "bliss_allocator_util.h"
#include "bliss_allocator_debug_util.h"


// Print the allocations
#define BR "+------------------------------+\n"
void bliss_dbg_print_block(bliss_block_t *block)
{
    bliss_block_idx_t idx;

    idx = bliss_a2i(block);

    printf(BR);
    printf("| idx: %-6u (%p) |\n", (unsigned int)idx, block);
    printf("|   nextblock: %-15u |\n", (unsigned int)block->next_block);
    printf("|   nextfree: %-15u  |\n", (unsigned int)block->next_free_block);
    printf(BR);
}

// Print the first n blocks
void bliss_dbg_print_blocks(size_t n)
{
    bliss_block_t *block;

    for (bliss_block_idx_t i=0; i<n; i++) {
        block = bliss_i2a(i);
        bliss_dbg_print_block(block);
    }
}

// Print all initialized blocks
void bliss_dbg_print_initialized_blocks(void)
{
    bliss_dbg_print_blocks(bliss_active_allocator->n_initialized_blocks);
}

// Print all blocks
void bliss_dbg_print_all_blocks(void)
{
    bliss_dbg_print_blocks(bliss_number_of_blocks);
}

void bliss_dbg_print_active_allocator(void)
{
    printf("Active allocator ptr: %p\n", bliss_active_allocator);
    printf("\t- Next free block: %p\n", bliss_active_allocator->next_free_block);
    printf("\t- N free blocks: %d\n", bliss_active_allocator->n_free_blocks);
    printf("\t- N initialized blocks: %d\n", bliss_active_allocator->n_initialized_blocks);
}

#endif /* BLISS_ALLOCATOR_DEBUG_UTIL_H_ */
