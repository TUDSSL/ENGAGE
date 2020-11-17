#include <stdlib.h>
#include <stdint.h>
//#include <stdio.h>

//#include <assert.h>

#include "bliss_allocator.h"
#include "bliss_allocator_nvm.h"
#include "bliss_allocator_util.h"

#include "checkpoint.h"

#ifdef BLISS_CUSTOM_MEMORY
bliss_block_t *bliss_memory_start = NULL;
bliss_block_idx_t bliss_number_of_blocks = 0;
#else
const bliss_block_t *const bliss_memory_start = (bliss_block_t *)(BLISS_START_ADDRESS);
const bliss_block_t *const bliss_memory_end = (bliss_block_t *)(BLISS_END_ADDRESS);
//bliss_block_idx_t bliss_number_of_blocks = (BLISS_END_ADDRESS-BLISS_START_ADDRESS)/(sizeof(bliss_block_t));
//bliss_block_idx_t bliss_number_of_blocks = bliss_memory_end-bliss_memory_start;
bliss_block_idx_t bliss_number_of_blocks;
#endif

CHECKPOINT_EXCLUDE_BSS
bliss_allocator_t bliss_allocator;

bliss_allocator_t *const bliss_active_allocator = &bliss_allocator;

#define BLISS_ACTIVE_IDX(l_)     (l_%2)
#define BLISS_INACTIVE_IDX(l_)   ((l_+1)%2)

/**
 * Debugging
 */
#define PRINT_LOG   (1)
#define PRINT_DEBUG (0)

#if PRINT_DEBUG
#include <stdio.h>
#define DEBUG_PRINT(...)    do {printf("[bliss-dbg] "); printf(__VA_ARGS__);} while (0)
#else
#define DEBUG_PRINT(...)
#endif

#if PRINT_LOG
#include <stdio.h>
#define LOG_PRINT(...)      do {printf("[bliss] "); printf(__VA_ARGS__);} while (0)
#else
#define LOG_PRINT(...)
#endif

size_t bliss_init(void)
{
    #ifndef BLISS_CUSTOM_MEMORY
    bliss_number_of_blocks = (BLISS_END_ADDRESS-BLISS_START_ADDRESS)/(sizeof(bliss_block_t));
    //bliss_number_of_blocks = 10;

    LOG_PRINT("Total memory [%p,%p] size: %d, block size: %d, block data size: %d\n",
              bliss_memory_start, bliss_memory_end,
              (int)(BLISS_END_ADDRESS-BLISS_START_ADDRESS),
              (int)sizeof(bliss_block_t),
              (int)BLISS_BLOCK_DATA_SIZE
              );
    #endif

    LOG_PRINT("Total number of blocks: %d\n", (int)bliss_number_of_blocks);

    bliss_allocator_t default_bliss_allocator = {
        .next_free_block = (bliss_block_t *)bliss_memory_start,
        .n_free_blocks = bliss_number_of_blocks,
        .n_initialized_blocks = 0
    };

    bliss_allocator_nvm[0] = default_bliss_allocator;
    bliss_allocator_nvm[1] = default_bliss_allocator;

    *bliss_active_allocator = default_bliss_allocator;

    return bliss_number_of_blocks;
}

/**
 * Initialize the active allocator, required for bouble buffering and
 * intermittent computing
 */
int bliss_restore(bliss_lclock_t lclock)
{
    /* Copy the previous allocator state to the active allocator state */
    *bliss_active_allocator = bliss_allocator_nvm[BLISS_INACTIVE_IDX(lclock)];
    return 0;
}

int bliss_checkpoint(bliss_lclock_t lclock)
{
    /* Commit the allocator state */
    bliss_allocator_nvm[BLISS_ACTIVE_IDX(lclock)] = *bliss_active_allocator;
    return 0;
}

/**
 * After a checkpoint the allocator state needs to be copied to the now active
 * allocator state (to continue with it)
 */
int bliss_post_checkpoint(bliss_lclock_t lclock)
{
    bliss_restore(lclock);
    return 0;
}

static bliss_block_t *bliss_alloc_block(void)
{
    // If there are no initialized blocks, add a block to the free list
    // i.e. initialize it
    bliss_block_idx_t n_iblocks = bliss_active_allocator->n_initialized_blocks;
    if (n_iblocks < bliss_number_of_blocks) {
        bliss_block_t *new_free_block = bliss_i2a(n_iblocks);
        n_iblocks += 1;
        new_free_block->next_free_block = n_iblocks;
        bliss_active_allocator->n_initialized_blocks = n_iblocks;
    }

    bliss_block_t *new_block = NULL;

    if (bliss_active_allocator->n_free_blocks > 0) {
        new_block = bliss_active_allocator->next_free_block;
        bliss_active_allocator->n_free_blocks -= 1;
        if (bliss_active_allocator->n_free_blocks != 0) {
            bliss_active_allocator->next_free_block = bliss_i2a(new_block->next_free_block);
        } else {
            bliss_active_allocator->next_free_block = NULL;
        }
    }
    DEBUG_PRINT("Alloc -> Free blocks: %d\n", bliss_active_allocator->n_free_blocks);

    return new_block;
}

static void bliss_free_block(bliss_block_t *free_block)
{
    if (bliss_active_allocator->next_free_block != NULL) {
        free_block->next_free_block = bliss_a2i(bliss_active_allocator->next_free_block);
    } else {
        free_block->next_free_block = bliss_number_of_blocks;
    }
    bliss_active_allocator->next_free_block = free_block;
    bliss_active_allocator->n_free_blocks += 1;
    DEBUG_PRINT("Free -> Free blocks: %d\n", bliss_active_allocator->n_free_blocks);
}

/**
 *Allocate a bliss list that can hold 'size' bytes of data
 */
bliss_list_t *bliss_alloc(size_t size)
{
    bliss_list_t *bliss_list;
    bliss_block_t *last_block;

    // Compute number of required blocks
    bliss_block_idx_t n_blocks = bliss_size2blocks(size);

    if (n_blocks > bliss_active_allocator->n_free_blocks) {
        return NULL;
    }

    // Allocate the first block seperately because it will be the block list
    // start (to avoid NULL comparisons in the loop)
    bliss_list = bliss_alloc_block();
    last_block = bliss_list;
    n_blocks -= 1;

    for (int i=0; i<n_blocks; i++) {
        bliss_block_t *new_block = bliss_alloc_block();
        //assert(new_block != NULL); // Debug assert
        if (new_block == NULL) {
            /* Ran out of memory! */
            return NULL;
        }
        last_block->next_block = bliss_a2i(new_block);
        //printf("link block index: %d to block %d\n", last_block->next_block, bliss_a2i(last_block));
        last_block = new_block;
    }

    last_block->next_block = BLISS_IDX_NULL; // End the list

    return bliss_list;
}

/**
 * Free a bliss list
 * Iterate through the bliss list and free all the blocks
 */
void bliss_free(bliss_list_t *free_list)
{
    bliss_block_t *this_free;
    bliss_block_idx_t free_block_idx;

    free_block_idx = bliss_a2i(free_list);

    while (free_block_idx != BLISS_IDX_NULL) {
        this_free = bliss_i2a(free_block_idx);
        free_block_idx = this_free->next_block;
        bliss_free_block(this_free);
    }
}

/**
 * Converts a pointer to somewhere in the block
 * to the bliss_block_t pointer
 * TODO: Merge with a2i?
 */
static inline bliss_block_t *bliss_ptr2start(void *ptr_in_block)
{
    uintptr_t block_ptr_val = (uintptr_t)ptr_in_block;
    block_ptr_val -= (uintptr_t)bliss_memory_start;

    block_ptr_val /= sizeof(bliss_block_t);

    return bliss_i2a(block_ptr_val);
}

/**
 * Copy data into a bliss list
 */
void bliss_store(void *bliss_lst, char *src, size_t n)
{
    bliss_block_idx_t n_blocks;
    size_t first_block_offset;
    size_t copy_first_block;
    size_t copy_last_block; // The number of bytes to copy to the last block

    /* The bliss_list pointer may be anywhere in the block
     * This is to allow for bliss to be a dropin for malloc when
     * a flexible array member is used
     * We DO assume that it's in the first block
     */
    bliss_list_t *bliss_list = bliss_ptr2start(bliss_lst);
    first_block_offset = (uintptr_t)bliss_lst - (uintptr_t)bliss_list;

    n_blocks = bliss_size2blocks(n + first_block_offset);


    // The first block is also the last
    if (n_blocks == 1) {
        copy_first_block = n;
    } else {
        copy_first_block = BLISS_BLOCK_DATA_SIZE - first_block_offset;
    }

    /* Copy the first block */
    bliss_memcpy(bliss_lst, src, copy_first_block);
    bliss_list = bliss_i2a(bliss_list->next_block);
    src = &src[copy_first_block];

    if (n_blocks == 1) {
        return;
    }

    /* Fill the first 1 - n_blocks-1 */
    for (int i=1; i<n_blocks-1; i++) {
        bliss_memcpy(bliss_list->data, src, BLISS_BLOCK_DATA_SIZE);
        bliss_list = bliss_i2a(bliss_list->next_block);
        src = &src[BLISS_BLOCK_DATA_SIZE]; // Move the src pointer
    }

    copy_last_block = (n + first_block_offset) % BLISS_BLOCK_DATA_SIZE;
    /* Copy the remaining data into the last block */
    bliss_memcpy(bliss_list->data, src, copy_last_block);
}

/**
 * Extract data from a bliss list
 */
void bliss_extract_woffset(char *dst, void *bliss_lst, size_t offset, size_t n)
{
    bliss_block_idx_t n_blocks, skip_blocks;
    size_t first_block_offset;
    size_t copy_first_block;
    size_t copy_last_block; // The number of bytes to copy to the last block

    /* The bliss_list pointer may be anywhere in the block
     * This is to allow for bliss to be a dropin for malloc when
     * a flexible array member is used
     * We DO assume that it's in the first block
     */
    bliss_list_t *bliss_list = bliss_ptr2start(bliss_lst);
    first_block_offset = (uintptr_t)bliss_lst - (uintptr_t)bliss_list;

    // Compute the total skip offset
    offset += first_block_offset;

    first_block_offset = offset % BLISS_BLOCK_DATA_SIZE;

    // How many blocks can we skip
    skip_blocks = offset / BLISS_BLOCK_DATA_SIZE;
    // The number of blocks we walk trough
    n_blocks = bliss_size2blocks(n + offset) - skip_blocks;

    // Sip untocuhed blocks
    while (skip_blocks) {
        bliss_list = bliss_i2a(bliss_list->next_block);
        skip_blocks--;
    }

    // The first block is also the last
    if (n_blocks == 1) {
        copy_first_block = n;
    } else {
        copy_first_block = BLISS_BLOCK_DATA_SIZE - first_block_offset;
    }

    /* Copy the first block */
    bliss_memcpy(dst, &bliss_list->data[first_block_offset], copy_first_block);
    bliss_list = bliss_i2a(bliss_list->next_block);
    dst = &dst[copy_first_block];

    if (n_blocks == 1) {
        return;
    }

    /* Fill the first 1 - n_blocks-1 */
    for (int i=1; i<n_blocks-1; i++) {
        bliss_memcpy(dst, bliss_list->data, BLISS_BLOCK_DATA_SIZE);
        bliss_list = bliss_i2a(bliss_list->next_block);
        dst = &dst[BLISS_BLOCK_DATA_SIZE]; // Move the dst pointer
    }

    copy_last_block = (n + first_block_offset) % BLISS_BLOCK_DATA_SIZE;
    /* Copy the remaining data into the last block */
    bliss_memcpy(dst, bliss_list->data, copy_last_block);
}

/**
 * Extract data from a bliss list
 */
void bliss_extract(char *dst, void *bliss_lst, size_t n)
{
    bliss_extract_woffset(dst, bliss_lst, 0, n);
}
