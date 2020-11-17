#ifndef BLISS_ALLOCATOR_H_
#define BLISS_ALLOCATOR_H_

#include <stdlib.h>

#include "bliss_allocator_cfg.h"

/**
 * Index 0xFFFF is special and represents NULL
 * This is used to signal the end of the chain
 */
#define BLISS_IDX_NULL 0xFFFF

/**
 * A BLISS data block of BLISS_BLOCK_DATA_SIZE
 * The minimum BLISS_BLOCK_DATA_SIZE is sizeof(bliss_block_idx_t)
 */
typedef struct bliss_block {
    union {
        uint8_t data[BLISS_BLOCK_DATA_SIZE];
        struct {
            uint8_t skip_bytes[BLISS_FIRST_BLOCK_SKIP]; // Skip metadata bytes
            //bliss_block_idx_t next_free_block;
        };
    };
    bliss_block_idx_t next_free_block;
    bliss_block_idx_t next_block; // The next block in the BLISS list
} bliss_block_t;

/**
 * Main "class" information of the allocator
 */
typedef struct bliss_allocator {
    bliss_block_t *next_free_block;         // A pointer to the next free block
    bliss_block_idx_t n_free_blocks;        // The number of free blocks in the pool
    bliss_block_idx_t n_initialized_blocks; //The number of initialized blocks
} bliss_allocator_t;


/**
 * An alias for clarity
 */
typedef bliss_block_t bliss_list_t;


/**
 * Initialize the bliss allocator
 */
size_t bliss_init(void);

/**
 * Restore the bliss allocator after a power-failure
 */
int bliss_restore(bliss_lclock_t lclock);

/**
 * Checkpoint the bliss allocator state
 */
int bliss_checkpoint(bliss_lclock_t lclock);

/**
 * Copy the checkpointed allocator state to the new allocator state
 * Required for double buffering
 */
int bliss_post_checkpoint(bliss_lclock_t lclock);

/**
 *Allocate a bliss list that can hold 'size' bytes of data
 */
bliss_list_t *bliss_alloc(size_t size);

/**
 * Free a bliss list
 */
void bliss_free(bliss_list_t *free);

/**
 * Copy data into a bliss list
 */
void bliss_store(void *bliss_list, char *src, size_t n);

/**
 * Extract data from a bliss list
 */
void bliss_extract(char *dst, void *bliss_list, size_t n);

/**
 * Extract data from a bliss list with a byte offset
 */
void bliss_extract_woffset(char *dst, void *bliss_list, size_t offset, size_t n);

#endif /* BLISS_ALLOCATOR_H_ */
