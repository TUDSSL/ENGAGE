#include "testcommon.h"

#ifndef BLISS_CUSTOM_MEMORY
#define BLISS_CUSTOM_MEMORY
#endif

#include "bliss_allocator.h"
#include "bliss_allocator_nvm.h"
#include "bliss_allocator_util.h"
#include "bliss_allocator_debug_util.h"

bliss_lclock_t lclock = 0;

void fake_checkpoint(void)
{
    bliss_checkpoint(lclock);
    lclock += 1;
    bliss_post_checkpoint(lclock);
}

void fake_powerfailure(void)
{
    // Clock stays the same
    bliss_restore(lclock);
}

/**
 * One time initialization
 * Used for debugging when the bliss memory is dynamically allocated
 * (For example during unit-tests)
 * Requires BLISS_START_ADDRESS to NOT be set in bliss_allocator_cfg.h
 */
// allocate 5 blocks
#define BLISS_DEBUG_MEMORY_SIZE (sizeof(bliss_block_t)*5)
int bliss_setup(void)
{
    bliss_memory_start = malloc(BLISS_DEBUG_MEMORY_SIZE);
    assert_true(bliss_memory_start != NULL);
    memset(bliss_memory_start, 0xFF, BLISS_DEBUG_MEMORY_SIZE);
    bliss_number_of_blocks = BLISS_DEBUG_MEMORY_SIZE/sizeof(bliss_block_t);

    bliss_allocator_t default_bliss_allocator = {
        .next_free_block = bliss_memory_start,
        .n_free_blocks = bliss_number_of_blocks,
        .n_initialized_blocks = 0
    };

    lclock = 0;
    bliss_allocator_nvm[0] = default_bliss_allocator;
    bliss_allocator_nvm[1] = default_bliss_allocator;

    bliss_restore(lclock);

    return 0;
}

int bliss_teardown(void)
{
    free(bliss_memory_start);

    return 0;
}


/**
 * Test setup and teardown
 */

int setup(void **state)
{
    (void)state;
    bliss_setup();
    return 0;
}

int teardown(void **state)
{
    (void)state;
    bliss_teardown();
    return 0;
}

/**
 * BLISS tests
 */

test(blocks_size_conversion)
{
    bliss_block_idx_t bsize = bliss_blocks2size(1);
    assert_true(bsize == BLISS_BLOCK_DATA_SIZE);

    bsize = bliss_size2blocks(1);
    assert_true(bsize == 1);

    bsize = bliss_size2blocks(BLISS_BLOCK_DATA_SIZE);
    assert_true(bsize == 1);

    bsize = bliss_size2blocks(BLISS_BLOCK_DATA_SIZE+1);
    assert_true(bsize == 2);
}

#if 0
test(logical_clock_active_buffer)
{
    bliss_allocator_t *astate_1, *astate_2, *astate_3;

    astate_1 = bliss_active_allocator;
    fake_checkpoint();
    astate_2 = bliss_active_allocator;

    // Should not be the same active allocator
    assert_true(astate_1 != astate_2);

    // There are two entries in the buffer, so the third one is the same as
    // the first one
    fake_checkpoint();
    astate_3 = bliss_active_allocator;
    assert_true(astate_2 != astate_3);
    assert_true(astate_1 == astate_3);

    assert_true(lclock != 0);
}
#endif

test(cmocka_setup_teardown)
{
    assert_true(lclock == 0);
}

test(allocate_next_free_block)
{
    bliss_list_t *bl;

    bliss_block_t *block_to_be_allocated = bliss_active_allocator->next_free_block;
    // The first free block should be allocated

    bl = bliss_alloc(1);
    assert_false(bl == NULL);

    assert_true(bl == block_to_be_allocated);
}

test(allocate_size_1)
{
    bliss_list_t *bl;

    bl = bliss_alloc(1);
    assert_false(bl == NULL);

    // Check if only one block was allocated
    bliss_block_idx_t next_block;
    next_block = bl->next_block;

    assert_true(next_block == BLISS_IDX_NULL);
}

test(allocate_block_size)
{
    bliss_list_t *bl;

    bl = bliss_alloc(BLISS_BLOCK_DATA_SIZE);
    assert_false(bl == NULL);

    // Check if only one block was allocated
    bliss_block_idx_t next_block;
    next_block = bl->next_block;

    assert_true(next_block == BLISS_IDX_NULL);
}

test(allocate_block_size_plus_one)
{
    bliss_list_t *bl;

    bl = bliss_alloc(BLISS_BLOCK_DATA_SIZE+1);
    assert_false(bl == NULL);

    // Check if two blocks where allocated
    bliss_block_idx_t next_block;
    next_block = bl->next_block;

    assert_false(next_block == BLISS_IDX_NULL);

    //bliss_dbg_print_blocks(3);

    bliss_block_t *b = bliss_i2a(next_block);

    // The second block should be the last block
    next_block = b->next_block;
    assert_true(next_block == BLISS_IDX_NULL);
}

test(allocate_all_blocks)
{
    //bliss_block_t *blocks[bliss_number_of_blocks];
    bliss_block_t **blocks;

    blocks = malloc(sizeof(bliss_block_t *) * bliss_number_of_blocks);

    // Allocate all the blocks
    for (int i=0; i<bliss_number_of_blocks; i++) {
        blocks[i] = bliss_alloc(1); // size of 1 gives 1 block
        assert_true(blocks[i] != NULL);
    }

    free(blocks);
}

test(allocate_all_blocks_plus_one)
{
    //bliss_block_t *blocks[bliss_number_of_blocks];
    bliss_block_t **blocks;

    blocks = malloc(sizeof(bliss_block_t *) * bliss_number_of_blocks);

    // Allocate all the blocks
    for (int i=0; i<bliss_number_of_blocks; i++) {
        blocks[i] = bliss_alloc(1); // size of 1 gives 1 block
        assert_true(blocks[i] != NULL);
    }

    assert_true(bliss_active_allocator->n_free_blocks == 0);

    // The next alloc should give NULL
    bliss_block_t *fail_block = bliss_alloc(1);
    assert_true(fail_block == NULL);

    free(blocks);
}

test(allocate_and_free_all_blocks)
{
    //bliss_block_t *blocks[bliss_number_of_blocks];
    bliss_block_t **blocks;

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == 0);

    blocks = malloc(sizeof(bliss_block_t *) * bliss_number_of_blocks);

    // Allocate all the blocks
    for (int i=0; i<bliss_number_of_blocks; i++) {
        blocks[i] = bliss_alloc(1); // size of 1 gives 1 block
        assert_true(blocks[i] != NULL);
    }

    assert_true(bliss_active_allocator->n_free_blocks == 0);
    assert_true(bliss_active_allocator->n_initialized_blocks == bliss_number_of_blocks);

    // The next alloc should give NULL
    bliss_block_t *fail_block = bliss_alloc(1);
    assert_true(fail_block == NULL);

    for (int i=0; i<bliss_number_of_blocks; i++) {
        bliss_free(blocks[i]);
    }

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == bliss_number_of_blocks);

    free(blocks);
}

test(allocate_and_free_all_blocks_twice)
{
    //bliss_block_t *blocks[bliss_number_of_blocks];
    bliss_block_t **blocks;
    bliss_block_t *fail_block;

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == 0);

    blocks = malloc(sizeof(bliss_block_t *) * bliss_number_of_blocks);

    // Allocate all the blocks
    for (int i=0; i<bliss_number_of_blocks; i++) {
        blocks[i] = bliss_alloc(1); // size of 1 gives 1 block
        assert_true(blocks[i] != NULL);
    }

    assert_true(bliss_active_allocator->n_free_blocks == 0);
    assert_true(bliss_active_allocator->n_initialized_blocks == bliss_number_of_blocks);

    // The next alloc should give NULL
    fail_block = bliss_alloc(1);
    assert_true(fail_block == NULL);

    // Free all the blocks
    for (int i=0; i<bliss_number_of_blocks; i++) {
        bliss_free(blocks[i]);
    }

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == bliss_number_of_blocks);

    // Do it all a second time

    // Allocate all the blocks
    for (int i=0; i<bliss_number_of_blocks; i++) {
        blocks[i] = bliss_alloc(1); // size of 1 gives 1 block
        assert_true(blocks[i] != NULL);
    }

    assert_true(bliss_active_allocator->n_free_blocks == 0);
    assert_true(bliss_active_allocator->n_initialized_blocks == bliss_number_of_blocks);

    // The next alloc should give NULL
    fail_block = bliss_alloc(1);
    assert_true(fail_block == NULL);

    // Free all the blocks
    for (int i=0; i<bliss_number_of_blocks; i++) {
        bliss_free(blocks[i]);
    }

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == bliss_number_of_blocks);

    free(blocks);
}

test(allocate_some_and_free_all_blocks)
{
    //bliss_block_t *blocks[bliss_number_of_blocks];
    bliss_block_t **blocks;

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == 0);

    blocks = malloc(sizeof(bliss_block_t *) * bliss_number_of_blocks);

    // Allocate 3 of the blocks
    for (int i=0; i<3; i++) {
        blocks[i] = bliss_alloc(1); // size of 1 gives 1 block
        assert_true(blocks[i] != NULL);
    }

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks-3);
    assert_true(bliss_active_allocator->n_initialized_blocks == 3);

    for (int i=0; i<3; i++) {
        bliss_free(blocks[i]);
    }

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == 3);

    free(blocks);
}

test(store_and_extract)
{
    // Use 2.5 blocks for the data
    size_t data_size = BLISS_BLOCK_DATA_SIZE * 2 + BLISS_BLOCK_DATA_SIZE / 2;

    char *store_data = malloc(data_size);
    char *compare_data = malloc(data_size);

    for (int i=0; i<data_size; i++) {
        store_data[i] = i;
    }

    bliss_list_t *d = bliss_alloc(data_size); // Should require 3 blocks

    // Store the data in the bliss list
    bliss_store(d, store_data, data_size);

    // Extract the data
    bliss_extract(compare_data, d, data_size);

    // Compare the data
    for (int i=0; i<data_size; i++) {
        assert_true(store_data[i] == compare_data[i]);
    }

    free(store_data);
    free(compare_data);
}

test(cast_struct_zero_size_array_store_extract)
{

    struct container {
        int var_a;
        int var_b;
        char data[];
    };

    // Use 2.5 blocks for the data
    size_t data_size = BLISS_BLOCK_DATA_SIZE * 2 + BLISS_BLOCK_DATA_SIZE / 2;

    char *store_data = malloc(data_size);
    char *compare_data = malloc(data_size);

    for (int i=0; i<data_size; i++) {
        store_data[i] = i;
    }

    bliss_list_t *d = bliss_alloc(sizeof(struct container) + data_size);
    struct container *cont = (struct container *)d;

    cont->var_a = 42;
    cont->var_b = 21;

    // Store the data in the bliss list
    bliss_store(cont->data, store_data, data_size);

    // Extract the data
    bliss_extract(compare_data, cont->data, data_size);

    assert_true(cont->var_a == 42);
    assert_true(cont->var_b == 21);

    // Compare the data
    for (int i=0; i<data_size; i++) {
        assert_true(store_data[i] == compare_data[i]);
    }

    free(store_data);
    free(compare_data);
}

// It's all in the first block, test store and extract
// block offset for first and last
test(cast_struct_zero_size_array_store_extract_one_block)
{

    struct container {
        int var_a;
        int var_b;
        char data[];
    };

    // Use < 1 block for the data
    size_t data_size =10;

    assert_true(data_size + sizeof(struct container) < BLISS_BLOCK_DATA_SIZE);


    char *store_data = malloc(data_size);
    char *compare_data = malloc(data_size);

    for (int i=0; i<data_size; i++) {
        store_data[i] = i;
    }

    bliss_list_t *d = bliss_alloc(sizeof(struct container) + data_size);
    struct container *cont = (struct container *)d;

    cont->var_a = 42;
    cont->var_b = 21;

    // Store the data in the bliss list
    bliss_store(cont->data, store_data, data_size);

    // Extract the data
    bliss_extract(compare_data, cont->data, data_size);

    assert_true(cont->var_a == 42);
    assert_true(cont->var_b == 21);

    // Compare the data
    for (int i=0; i<data_size; i++) {
        assert_true(store_data[i] == compare_data[i]);
    }

    free(store_data);
    free(compare_data);
}

test(cast_struct_zero_size_array_store_extract_woffset_oneblock)
{

    struct container {
        int var_a;
        int var_b;
        char data[];
    };

    // Use 2.5 blocks for the data
    size_t data_size = BLISS_BLOCK_DATA_SIZE * 2 + BLISS_BLOCK_DATA_SIZE / 2;

    char *store_data = malloc(data_size);
    char *compare_data = malloc(data_size);

    for (int i=0; i<data_size; i++) {
        store_data[i] = i;
    }

    bliss_list_t *d = bliss_alloc(sizeof(struct container) + data_size);
    struct container *cont = (struct container *)d;

    cont->var_a = 42;
    cont->var_b = 21;

    // Store the data in the bliss list
    bliss_store(cont->data, store_data, data_size);

    size_t offset = BLISS_BLOCK_DATA_SIZE+10;
    size_t offset_comp_size = 10;

    // Extract the data
    bliss_extract_woffset(&compare_data[offset], cont->data, offset, offset_comp_size);

    assert_true(cont->var_a == 42);
    assert_true(cont->var_b == 21);

    // Compare the data
    for (int i=offset; i<offset+offset_comp_size; i++) {
        //printf("index: %d -- got %d expected %d\n", i, compare_data[i], store_data[i]);
        assert_true(store_data[i] == compare_data[i]);
    }

    free(store_data);
    free(compare_data);
}

test(cast_struct_zero_size_array_store_extract_woffset_multiblock)
{

    struct container {
        int var_a;
        int var_b;
        char data[];
    };

    // Use 2.5 blocks for the data
    size_t data_size = BLISS_BLOCK_DATA_SIZE * 2 + BLISS_BLOCK_DATA_SIZE / 2;

    char *store_data = malloc(data_size);
    char *compare_data = malloc(data_size);

    for (int i=0; i<data_size; i++) {
        store_data[i] = i;
    }

    bliss_list_t *d = bliss_alloc(sizeof(struct container) + data_size);
    struct container *cont = (struct container *)d;

    cont->var_a = 42;
    cont->var_b = 21;

    // Store the data in the bliss list
    bliss_store(cont->data, store_data, data_size);

    size_t offset = BLISS_BLOCK_DATA_SIZE+10;
    size_t offset_comp_size = BLISS_BLOCK_DATA_SIZE;

    // Extract the data
    bliss_extract_woffset(&compare_data[offset], cont->data, offset, offset_comp_size);

    assert_true(cont->var_a == 42);
    assert_true(cont->var_b == 21);

    // Compare the data
    for (int i=offset; i<offset+offset_comp_size; i++) {
        //printf("index: %d -- got %d expected %d\n", i, compare_data[i], store_data[i]);
        assert_true(store_data[i] == compare_data[i]);
    }

    free(store_data);
    free(compare_data);
}

test(double_free)
{
    // We must be able to free bloks without sideaffects, so a double free
    // is allowed

    // Use 2.5 blocks for the data
    size_t data_size = BLISS_BLOCK_DATA_SIZE * 2 + BLISS_BLOCK_DATA_SIZE / 2;
    bliss_list_t *d = bliss_alloc(data_size);

    assert_true(d != NULL);

    fake_checkpoint();

    bliss_block_idx_t n_free_blocks_before_free = bliss_active_allocator->n_free_blocks;

    // First free
    bliss_free(d);

    //printf("\nAfter free\n");
    //bliss_dbg_print_active_allocator();
    //bliss_dbg_print_blocks(4);

    bliss_block_idx_t n_free_blocks_after_free = bliss_active_allocator->n_free_blocks;
    bliss_block_idx_t n_init_blocks_after_free = bliss_active_allocator->n_initialized_blocks;
    bliss_block_t *next_free_after_free = bliss_active_allocator->next_free_block;

    assert_true(n_free_blocks_before_free < n_free_blocks_after_free);

    fake_powerfailure();

    // Second free
    bliss_free(d);

    //printf("\nAfter SECOND free\n");
    //bliss_dbg_print_active_allocator();
    //bliss_dbg_print_blocks(4);

    bliss_block_idx_t n_free_blocks_after_second_free = bliss_active_allocator->n_free_blocks;
    bliss_block_idx_t n_init_blocks_after_second_free = bliss_active_allocator->n_initialized_blocks;
    bliss_block_t *next_free_after_second_free = bliss_active_allocator->next_free_block;

    assert_true(n_free_blocks_after_free == n_free_blocks_after_second_free);
    assert_true(n_init_blocks_after_free == n_init_blocks_after_second_free);
    assert_true(next_free_after_free == next_free_after_second_free);

}

static inline void compare_allocators(bliss_allocator_t *a1, bliss_allocator_t *a2)
{
    /* Check if the allocator state was maintained across the checkpoint */
    assert_true(a1->next_free_block == a2->next_free_block);
    assert_true(a1->n_free_blocks == a2->n_free_blocks);
    assert_true(a1->n_initialized_blocks == a2->n_initialized_blocks);
}

/*
 * Intermittent tests
 */
test(intermittent_clock_increase)
{
    /* After a clock increase the state must be kept */
    bliss_block_t *block = bliss_alloc(1);

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks-1);
    assert_true(bliss_active_allocator->n_initialized_blocks == 1);

    bliss_allocator_t allocator_before_clock_change = *bliss_active_allocator;
    bliss_allocator_t *active_allocator_before_clock_change = bliss_active_allocator;

    fake_checkpoint();

    // Changed to volatile active allocator
    //assert_true(bliss_active_allocator != active_allocator_before_clock_change);

    /* Check if the allocator state was maintained across the checkpoint */
    compare_allocators(bliss_active_allocator, &allocator_before_clock_change);

    /* Check if the restore allocator is equal to the current allocator (no allocations where made) */
    compare_allocators(bliss_active_allocator, active_allocator_before_clock_change);

    bliss_free(block);

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == 1);
}

test(intermittent_clock_increase_multiple_times)
{
    /* After a clock increase the state must be kept */
    bliss_block_t *block = bliss_alloc(1);

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks-1);
    assert_true(bliss_active_allocator->n_initialized_blocks == 1);

    bliss_allocator_t allocator_before_clock_change = *bliss_active_allocator;
    bliss_allocator_t *active_allocator_before_clock_change = bliss_active_allocator;

    fake_checkpoint();

    // Changed to volatile active allocator
    //assert_true(bliss_active_allocator != active_allocator_before_clock_change);

    /* Check if the allocator state was maintained across the checkpoint */
    compare_allocators(bliss_active_allocator, &allocator_before_clock_change);

    /* Check if the restore allocator is equal to the current allocator (no allocations where made) */
    compare_allocators(bliss_active_allocator, active_allocator_before_clock_change);

    bliss_free(block);

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == 1);

    bliss_allocator_t allocator_before_second_clock_change = *bliss_active_allocator;
    bliss_allocator_t *active_allocator_before_second_clock_change = bliss_active_allocator;

    fake_checkpoint();

    /* We are back to the origional buffer */
    // Changed to volatile active allocator
    //assert_true(bliss_active_allocator == active_allocator_before_clock_change);

    /* Check if the allocator state was maintained across the checkpoint */
    compare_allocators(bliss_active_allocator, &allocator_before_second_clock_change);

    /* Check if the restore allocator is equal to the current allocator (no allocations where made) */
    compare_allocators(bliss_active_allocator, active_allocator_before_second_clock_change);
}

test(intermittent_powerfail)
{
    /* After a clock increase the state must be kept */
    bliss_block_t *block = bliss_alloc(1);

    fake_checkpoint();

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks-1);
    assert_true(bliss_active_allocator->n_initialized_blocks == 1);

    bliss_allocator_t allocator_after_checkpoint = *bliss_active_allocator;

    bliss_free(block);

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == 1);

    bliss_allocator_t allocator_before_restore = *bliss_active_allocator;
    bliss_allocator_t *active_before_restore = bliss_active_allocator;

    // Now the power fails and we restore
    fake_powerfailure();

    assert_true(active_before_restore == bliss_active_allocator);

    // Check if the state is restored to the state after the checkpoint
    compare_allocators(bliss_active_allocator, &allocator_after_checkpoint);

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks-1);
    assert_true(bliss_active_allocator->n_initialized_blocks == 1);

    // Now redo the actions
    bliss_free(block);

    assert_true(bliss_active_allocator->n_free_blocks == bliss_number_of_blocks);
    assert_true(bliss_active_allocator->n_initialized_blocks == 1);

    // Check if the state is now equal to the state before the powerfailure
    compare_allocators(bliss_active_allocator, &allocator_before_restore);
}


/*
* Register Tests
*/
#define bliss_cmocka_unit_test(t_) cmocka_unit_test_setup_teardown(t_, setup, teardown)
int main(void)
{
    const struct CMUnitTest tests[] = {
        /* Allocation tests */
        bliss_cmocka_unit_test(blocks_size_conversion),
        //bliss_cmocka_unit_test(logical_clock_active_buffer), // Changed to volatile active buffer
        bliss_cmocka_unit_test(cmocka_setup_teardown),
        bliss_cmocka_unit_test(allocate_next_free_block),
        bliss_cmocka_unit_test(allocate_size_1),
        bliss_cmocka_unit_test(allocate_block_size),
        bliss_cmocka_unit_test(allocate_block_size_plus_one),
        bliss_cmocka_unit_test(allocate_all_blocks),
        bliss_cmocka_unit_test(allocate_all_blocks_plus_one),
        bliss_cmocka_unit_test(allocate_and_free_all_blocks),
        bliss_cmocka_unit_test(allocate_and_free_all_blocks_twice),
        bliss_cmocka_unit_test(allocate_some_and_free_all_blocks),
        bliss_cmocka_unit_test(store_and_extract),
        bliss_cmocka_unit_test(cast_struct_zero_size_array_store_extract),
        bliss_cmocka_unit_test(cast_struct_zero_size_array_store_extract_one_block),
        bliss_cmocka_unit_test(cast_struct_zero_size_array_store_extract_woffset_oneblock),
        bliss_cmocka_unit_test(cast_struct_zero_size_array_store_extract_woffset_multiblock),
        bliss_cmocka_unit_test(double_free),

        /* Intermittency tests */
        bliss_cmocka_unit_test(intermittent_clock_increase),
        bliss_cmocka_unit_test(intermittent_clock_increase_multiple_times),
        bliss_cmocka_unit_test(intermittent_powerfail),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
