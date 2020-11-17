#ifndef BLISS_ALLOCATOR_UTIL_H_
#define BLISS_ALLOCATOR_UTIL_H_

#include "bliss_allocator_cfg.h"
#include "bliss_allocator.h"

#ifdef BLISS_CUSTOM_MEMORY
extern bliss_block_t *bliss_memory_start;
extern bliss_block_idx_t bliss_number_of_blocks;
#else
extern const bliss_block_t *const bliss_memory_start;
extern bliss_block_idx_t bliss_number_of_blocks;
#endif

extern bliss_allocator_t *const bliss_active_allocator;

/**
 * Index to address conversion
 */
static inline bliss_block_t *bliss_i2a(bliss_block_idx_t idx)
{
    return (bliss_block_t *)(&bliss_memory_start[idx]);
}

/**
 * Address to index conversion
 */
static inline bliss_block_idx_t bliss_a2i(bliss_block_t *addr)
{
    // NB. Because the struct size is know we dont need to divide by sizeof
    return (bliss_block_idx_t)(addr-bliss_memory_start);
}

/**
 * Convert a size to the number of blocks required to hold 'size' bytes of data
 * Size has to be larger than 0.
 */
static inline bliss_block_idx_t bliss_size2blocks(size_t size)
{
    bliss_block_idx_t n_blocks;

    n_blocks = 1 + ((size-1)/BLISS_BLOCK_DATA_SIZE);

    return n_blocks;
}

static inline size_t bliss_blocks2size(bliss_block_idx_t blocks)
{
    return blocks * BLISS_BLOCK_DATA_SIZE;
}

#endif /* BLISS_ALLOCATOR_UTIL_H_ */
