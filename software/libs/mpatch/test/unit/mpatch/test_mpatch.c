#include "testcommon.h"

#ifndef BLISS_CUSTOM_MEMORY
#define BLISS_CUSTOM_MEMORY
#endif


#include "bliss_allocator.h"
#include "bliss_allocator_nvm.h"
#include "bliss_allocator_util.h"
#include "bliss_allocator_debug_util.h"

#include "mpatch.h"
#include "mpatch_util.h"
#include "mpatch_nvm.h"

volatile lclock_t lclock;
extern mpatch_lclock_t mpatch_lclock[2];

void fake_checkpoint(void)
{
    //printf("--CHECKPOINT--\n");
    mpatch_stage();
    mpatch_core_checkpoint(false);
    lclock += 1;
    mpatch_core_post_checkpoint();
}

void fake_checkpoint_nostage(void)
{
    mpatch_core_checkpoint(false);
    lclock += 1;
    mpatch_core_post_checkpoint();
}

void fake_powerfailure(void)
{
    //printf("--POWER FAILURE--\n");
    // Clock stays the same
    mpatch_core_restore();

    mpatch_recover();
}

void fake_powerfailure_restore(void)
{
    fake_powerfailure();
    mpatch_apply_all(false);
}

// In this test we will require REQUIRED_TEST_DATA_MEM of checkpoint data
#define REQUIRED_TEST_DATA_MEM (25*512)

#define REQUIRED_BLISS_BLOCKS (REQUIRED_TEST_DATA_MEM / BLISS_BLOCK_DATA_SIZE + 1)

/**
 * One time initialization
 * Used for debugging when the bliss memory is dynamically allocated
 * (For example during unit-tests)
 * Requires BLISS_START_ADDRESS to NOT be set in bliss_allocator_cfg.h
 */
// allocate 100 blocks
#define BLISS_DEBUG_MEMORY_SIZE (sizeof(bliss_block_t)*REQUIRED_BLISS_BLOCKS)
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

    bliss_allocator_nvm[0] = default_bliss_allocator;
    bliss_allocator_nvm[1] = default_bliss_allocator;

    bliss_restore(mpatch_active_lclock);

    return 0;
}

int bliss_teardown(void)
{
    free(bliss_memory_start);

    return 0;
}


int mpatch_setup(void)
{
    // Reset volatile memory
    //memset(mpatch_pending_patches, 0, sizeof(mpatch_pending_patches));
    //memset(mpatch_pending_patches, 0, sizeof(mpatch_pending_patches));

    //it_root = NULL;

    //// Reset non-volatile memory
    //lclock = 0;
    //mpatch_lclock[0] = 0;
    //mpatch_lclock[1] = 0;
    //memset(mpatch_patch_origin, 0, sizeof(mpatch_patch_origin));
    //memset(mpatch_pending_patches_nvm, 0, sizeof(mpatch_pending_patches_nvm));

    bliss_setup();

    del_modify_flag = 0;

    mpatch_lclock[0] = 0;
    mpatch_lclock[1] = 0;
    memset(mpatch_patch_origin, 0, sizeof(mpatch_patch_origin));
    memset(mpatch_pending_patches_nvm, 0, sizeof(mpatch_pending_patches_nvm));

    mpatch_core_restore();

    return 0;
}

int mpatch_teardown(void)
{
    bliss_teardown();
    return 0;
}


/**
 * Test setup and teardown
 */

int setup(void **state)
{
    (void)state;
    mpatch_setup();
    return 0;
}

int teardown(void **state)
{
    (void)state;
    mpatch_teardown();
    return 0;
}


/**
 * MPatch tests
 */


test(cmocka_setup_teardown)
{
    assert_true(lclock == 0);
}


test(get_pending_patch_from_id)
{
    mpatch_id_t id = MPATCH_FIRST_ID;

    mpatch_pending_patch_t *patch = mpatch_get(id);

    assert_true(patch == &mpatch_pending_patches[id]);
}

test(new_pending_region_cont)
{
    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);

    // patch should start disabled
    assert_true(patch->state == MPATCH_PATCH_DISABLED);

    mpatch_new_region(patch, 0, 100, MPATCH_CONTINUOUS);

    assert_true(patch->range.low == 0);
    assert_true(patch->range.high == 100);

    assert_true(patch->max_range.low == 0);
    assert_true(patch->max_range.high == 100);

    assert_true(patch->max_pending_range.low == 0);
    assert_true(patch->max_pending_range.high == 100);

    assert_true(patch->type == MPATCH_CONTINUOUS);

    // patch should still be disabled
    assert_true(patch->state == MPATCH_PATCH_DISABLED);

    mpatch_enable(patch);
    assert_true(patch->state == MPATCH_PATCH_ENABLED);
}

test(new_region_already_pending_patch)
{
    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);

    mpatch_new_region(patch, 0, 100, MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    assert_true(patch->state == MPATCH_PATCH_ENABLED);

    // Make a new region for the same patch
    mpatch_new_region(patch, 10, 20, MPATCH_TYPE_UNCHANGED);
    assert_true(patch->type == MPATCH_CONTINUOUS);

    assert_true(patch->range.low == 10);
    assert_true(patch->range.high == 20);

    assert_true(patch->max_range.low == 10);
    assert_true(patch->max_range.high == 20);

    assert_true(patch->max_pending_range.low == 10);
    assert_true(patch->max_pending_range.high == 20);

    // Patch is still enabled
    assert_true(patch->state == MPATCH_PATCH_ENABLED);
}

test(modify_region_subregion_already_pending_patch)
{
    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);

    mpatch_new_region(patch, 0, 100, MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    assert_true(patch->state == MPATCH_PATCH_ENABLED);

    // Modify the region to a smaller subregion
    mpatch_modify_region(patch, 10, 20, MPATCH_CONTINUOUS);

    assert_true(patch->range.low == 10);
    assert_true(patch->range.high == 20);

    // The pending range should still be the max range
    assert_true(patch->max_pending_range.low == 0);
    assert_true(patch->max_pending_range.high == 100);

    // Patch is still enabled
    assert_true(patch->state == MPATCH_PATCH_ENABLED);
}

test(modify_region_largerregion_already_pending_patch)
{
    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);

    mpatch_new_region(patch, 10, 90, MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    assert_true(patch->state == MPATCH_PATCH_ENABLED);

    // Modify the region to a smaller subregion
    mpatch_modify_region(patch, 0, 100, MPATCH_CONTINUOUS);

    assert_true(patch->range.low == 0);
    assert_true(patch->range.high == 100);

    // The pending range should still be the max range
    assert_true(patch->max_pending_range.low == 0);
    assert_true(patch->max_pending_range.high == 100);

    // Patch is still enabled
    assert_true(patch->state == MPATCH_PATCH_ENABLED);
}

test(modify_region_overlapregion_already_pending_patch)
{
    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);

    mpatch_new_region(patch, 0, 100, MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    assert_true(patch->state == MPATCH_PATCH_ENABLED);

    // Modify the region to a smaller subregion
    mpatch_modify_region(patch, 20, 120, MPATCH_CONTINUOUS);

    assert_true(patch->range.low == 20);
    assert_true(patch->range.high == 120);

    // The pending range should still be the max range
    assert_true(patch->max_pending_range.low == 0);
    assert_true(patch->max_pending_range.high == 120);

    // Patch is still enabled
    assert_true(patch->state == MPATCH_PATCH_ENABLED);
}

test(modify_region_otherregion_already_pending_patch)
{
    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);

    mpatch_new_region(patch, 0, 100, MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    assert_true(patch->state == MPATCH_PATCH_ENABLED);

    // Modify the region to a smaller subregion
    mpatch_modify_region(patch, 200, 300, MPATCH_CONTINUOUS);

    assert_true(patch->range.low == 200);
    assert_true(patch->range.high == 300);

    // The pending range should still be the max range
    assert_true(patch->max_pending_range.low == 0);
    assert_true(patch->max_pending_range.high == 300);

    // Patch is still enabled
    assert_true(patch->state == MPATCH_PATCH_ENABLED);
}

test(stage_single_new_region_cont)
{
    const size_t patch_size = 100;

    char *patch_content = malloc(patch_size);
    char *patch_extract = malloc(patch_size);

    for (int i=0; i<patch_size; i++) {
        patch_content[i] = i;
    }


    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[0],
            (mpatch_addr_t)&patch_content[patch_size-1],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    // Create the patches
    mpatch_stage();

    // Get the newly created nvm patch
    mpatch_origin_t *origin = &mpatch_active_patch_origin[MPATCH_FIRST_ID];
    mpatch_patch_t *nvm_patch = origin->patch_list;

    bliss_extract(patch_extract, nvm_patch->data, patch_size);

    // Compare the patch content
    for (int i=0; i<patch_size; i++) {
        assert_true(patch_content[i] == patch_extract[i]);
    }

    free(patch_content);
    free(patch_extract);
}


test(commit_restore_single_new_region_cont)
{
    const size_t patch_size = 100;

    char *patch_content = malloc(patch_size);
    char *patch_compare = malloc(patch_size);

    for (int i=0; i<patch_size; i++) {
        patch_content[i] = i;
        patch_compare[i] = i;
    }


    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[0],
            (mpatch_addr_t)&patch_content[patch_size-1],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    // Commit a checkpoint (stage and commit patches)
    fake_checkpoint();

    // Modify the patch_content
    for (int i=0; i<patch_size; i++) {
        patch_content[i] += 1;
        assert_true(patch_content[i] != patch_compare[i]);
    }

    // The content is now different from patch_compare
    // If we restore it now, it should be equal again
    mpatch_apply_all(false);

    // Compare the patch_content
    for (int i=0; i<patch_size; i++) {
        assert_true(patch_content[i] == patch_compare[i]);
    }

    free(patch_content);
    free(patch_compare);
}

test(commit_restore_single_new_region_cont_multiblock)
{
    const size_t patch_size = BLISS_BLOCK_DATA_SIZE+1;

    char *patch_content = malloc(patch_size);
    char *patch_compare = malloc(patch_size);

    for (int i=0; i<patch_size; i++) {
        patch_content[i] = i;
        patch_compare[i] = i;
    }

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[0],
            (mpatch_addr_t)&patch_content[patch_size-1],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    // Commit a checkpoint (stage and commit patches)
    fake_checkpoint();

    // Modify the patch_content
    for (int i=0; i<patch_size; i++) {
        patch_content[i] += 1;
        assert_true(patch_content[i] != patch_compare[i]);
    }

    // The content is now different from patch_compare
    // If we restore it now, it should be equal again
    mpatch_apply_all(false);

    // Commit a second checkpoint (stage and commit patches)
    fake_checkpoint();

    // Compare the patch_content
    for (int i=0; i<patch_size; i++) {
        assert_true(patch_content[i] == patch_compare[i]);
    }

    free(patch_content);
    free(patch_compare);
}

test(commit_restore_single_new_region_cont_multiblock_twice)
{
    const size_t patch_size = BLISS_BLOCK_DATA_SIZE+1;

    char *patch_content = malloc(patch_size);
    char *patch_compare = malloc(patch_size);

    for (int i=0; i<patch_size; i++) {
        patch_content[i] = i;
        patch_compare[i] = i;
    }

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[0],
            (mpatch_addr_t)&patch_content[patch_size-1],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    // Commit a checkpoint (stage and commit patches)
    fake_checkpoint();

    // Modify the patch_content and compare
    for (int i=0; i<patch_size; i++) {
        patch_content[i] += 1;
        patch_compare[i] = patch_content[i];
        assert_true(patch_content[i] == patch_compare[i]);
    }

    // Commit a second checkpoint (stage and commit patches)
    fake_checkpoint();

    // Modify the patch_content
    for (int i=0; i<patch_size; i++) {
        patch_content[i] += 1;
        assert_true(patch_content[i] != patch_compare[i]);
    }

    // The content is now different from patch_compare
    // If we restore it now, it should be equal again
    mpatch_apply_all(false);

    free(patch_content);
    free(patch_compare);
}

test(commit_restore_modify_region)
{
    const size_t patch_size = BLISS_BLOCK_DATA_SIZE+1;
    const size_t second_patch_size = patch_size/2;

    char *patch_content = malloc(patch_size);
    char *patch_compare = malloc(patch_size);

    for (int i=0; i<patch_size; i++) {
        patch_content[i] = i;
        patch_compare[i] = i;
    }

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[0],
            (mpatch_addr_t)&patch_content[patch_size-1],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    // Modify the region to be smaller
    // Will be triggered next checkpoint
    mpatch_modify_region(patch,
            (mpatch_addr_t)&patch_content[0],
            (mpatch_addr_t)&patch_content[second_patch_size-1],
            MPATCH_TYPE_UNCHANGED);

    // Commit a checkpoint (stage and commit patches)
    fake_checkpoint();


    // Modify the patch_content and compare of the sub region
    for (int i=0; i<second_patch_size; i++) {
        patch_content[i] += 1;
        patch_compare[i] = patch_content[i];
        assert_true(patch_content[i] == patch_compare[i]);
    }

    // Commit a second checkpoint (stage and commit patches)
    fake_checkpoint();

    // Modify the patch_content
    for (int i=0; i<patch_size; i++) {
        patch_content[i] += 1;
        assert_true(patch_content[i] != patch_compare[i]);
    }

    // The content is now different from patch_compare
    // If we restore it now, it should be equal again
    mpatch_apply_all(false);

    free(patch_content);
    free(patch_compare);
}

test(commit_disabled_region)
{
    const size_t patch_size = 100;

    char *patch_content = malloc(patch_size);
    char *patch_compare = malloc(patch_size);

    for (int i=0; i<patch_size; i++) {
        patch_content[i] = i;
        patch_compare[i] = i;
    }


    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[0],
            (mpatch_addr_t)&patch_content[patch_size-1],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    mpatch_disable(patch);

    assert_true(patch->state == MPATCH_PATCH_DISABLED);

    // Commit a checkpoint (stage and commit patches)
    fake_checkpoint();

    // Modify the patch_content
    for (int i=0; i<patch_size; i++) {
        patch_content[i] += 1;
        assert_true(patch_content[i] != patch_compare[i]);
    }

    // The content is now different from patch_compare
    // If we restore it now, it should be equal again
    mpatch_apply_all(false);

    // Compare the patch_content
    for (int i=0; i<patch_size; i++) {
        assert_false(patch_content[i] == patch_compare[i]);
    }

    free(patch_content);
    free(patch_compare);
}

test(commit_singleshot)
{
    const size_t patch_size = 100;

    char *patch_content = malloc(patch_size);
    char *patch_compare = malloc(patch_size);

    for (int i=0; i<patch_size; i++) {
        patch_content[i] = i;
        patch_compare[i] = i;
    }


    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[0],
            (mpatch_addr_t)&patch_content[patch_size-1],
            MPATCH_SINGLESHOT);
    mpatch_enable(patch);

    assert_true(patch->state == MPATCH_PATCH_ENABLED);

    // Commit a checkpoint (stage and commit patches)
    fake_checkpoint();

    assert_true(patch->state == MPATCH_PATCH_DISABLED);

    // Modify the patch_content
    for (int i=0; i<patch_size; i++) {
        patch_content[i] += 1;
        assert_true(patch_content[i] != patch_compare[i]);
    }

    // The content is now different from patch_compare
    // If we restore it now, it should be equal again
    mpatch_apply_all(false);

    // Compare the patch_content
    for (int i=0; i<patch_size; i++) {
        assert_true(patch_content[i] == patch_compare[i]);
    }

    // Modify the patch_content
    for (int i=0; i<patch_size; i++) {
        patch_content[i] += 2;
        assert_true(patch_content[i] != patch_compare[i]);
    }

    // Commit a checkpoint (stage and commit patches)
    // Should NOT checkpoint data, as the patch is now DISABLED
    fake_checkpoint();

    mpatch_apply_all(false);

    // Compare the patch_content (should be the same as the first)
    for (int i=0; i<patch_size; i++) {
        assert_true(patch_content[i] == patch_compare[i]);
    }

    free(patch_content);
    free(patch_compare);
}

static inline
void patch_overlap_base(size_t patch_size, size_t patch_1_low, size_t patch_1_high, size_t patch_2_low, size_t patch_2_high)
{
    char *patch_content = malloc(patch_size);
    char *patch_compare = malloc(patch_size);

    //printf("Patch 1 low: (%ld) high: (%ld)\n", (long)patch_content[patch

    for (int i=0; i<patch_size; i++) {
        patch_content[i] = i;
        patch_compare[i] = i;
    }

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[patch_1_low],
            (mpatch_addr_t)&patch_content[patch_1_high],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    mpatch_modify_region(patch,
            (mpatch_addr_t)&patch_content[patch_2_low],
            (mpatch_addr_t)&patch_content[patch_2_high],
            MPATCH_CONTINUOUS);

    // Commit a checkpoint (stage and commit patches)
    fake_checkpoint();

    // Modify the patch 2 content
    for (int i=patch_2_low; i<=patch_2_high; i++) {
        patch_content[i] += 10;
        patch_compare[i] = patch_content[i];
    }

    // Commit the checkpoint for patch_2
    fake_checkpoint();

    // Scramble the patch content
    for (int i=0; i<patch_size; i++) {
        patch_content[i] += 2;
    }

    // The content is now different from patch_compare
    // If we restore it now, it should be equal again
    mpatch_apply_all(false);


    // Compare the patch_content
    for (int i=0; i<patch_size; i++) {
        //printf("idx: %d -- [%d] exp: %d\n", i, patch_content[i], patch_compare[i]);
        assert_true(patch_content[i] == patch_compare[i]);
    }

    free(patch_content);
    free(patch_compare);
}


test(patch_overlap_subpatch_lower_same)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    const size_t patch_2_low = 0;
    const size_t patch_2_high = 50;

    patch_overlap_base(patch_size,
            patch_1_low, patch_1_high,
            patch_2_low, patch_2_high);
}

test(patch_overlap_subpatch_higher_same)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    const size_t patch_2_low = 50;
    const size_t patch_2_high = 99;

    patch_overlap_base(patch_size,
            patch_1_low, patch_1_high,
            patch_2_low, patch_2_high);
}

test(patch_overlap_subpatch_inside)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    const size_t patch_2_low = 20;
    const size_t patch_2_high = 80;

    patch_overlap_base(patch_size,
            patch_1_low, patch_1_high,
            patch_2_low, patch_2_high);
}

test(patch_overlap_total_overlap)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 20;
    const size_t patch_1_high = 30;

    const size_t patch_2_low = 0;
    const size_t patch_2_high = 99;

    patch_overlap_base(patch_size,
            patch_1_low, patch_1_high,
            patch_2_low, patch_2_high);
}

test(patch_overlap_modify_outside_higher)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 30;
    // Gap in the middle, should extend
    const size_t patch_2_low = 50;
    const size_t patch_2_high = 99;

    patch_overlap_base(patch_size,
            patch_1_low, patch_1_high,
            patch_2_low, patch_2_high);
}

test(patch_overlap_modify_outside_lower)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 50;
    const size_t patch_1_high = 99;
    // Gap in the middle, should extend
    const size_t patch_2_low = 0;
    const size_t patch_2_high = 39;

    patch_overlap_base(patch_size,
            patch_1_low, patch_1_high,
            patch_2_low, patch_2_high);
}

static void patch_obsolete_delete_check(void)
{
    /* Delete tests: */

    // During the apply, patch 1 should have been marked for delete

    // Get the newly created nvm patch
    mpatch_origin_t *origin = &mpatch_active_patch_origin[MPATCH_FIRST_ID];

    mpatch_patch_t *nvm_patch_2 = origin->patch_list; // newest patch (2)
    mpatch_patch_t *nvm_patch_1 = origin->patch_list->next; // old patch (1)

    assert_true(nvm_patch_1 != NULL);
    assert_true(nvm_patch_2 != NULL);

    // A checkpoint of the core system (not memory) is required before applying
    // actions (bliss and mpatch core)!
    // We DONT want to create the actual patches
    //fake_checkpoint_nostage();
    //fake_checkpoint();

    //mpatch_apply_actions_all();
    mpatch_sweep_delete_obselete();

    // Patch 1 should now be deleted
    mpatch_patch_t *nvm_patch_1_new = origin->patch_list->next; // old patch (1)
    assert_true(nvm_patch_1_new == NULL);

}

test(patch_obsolete_total_obsolete_inside)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 10;
    const size_t patch_1_high = 50;

    const size_t patch_2_low = 0;
    const size_t patch_2_high = 99;

    patch_overlap_base(patch_size,
            patch_1_low, patch_1_high,
            patch_2_low, patch_2_high);

    patch_obsolete_delete_check();
}

test(patch_obsolete_total_obsolete_same_low)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 50;

    const size_t patch_2_low = 0;
    const size_t patch_2_high = 99;

    patch_overlap_base(patch_size,
            patch_1_low, patch_1_high,
            patch_2_low, patch_2_high);

    patch_obsolete_delete_check();
}

test(patch_obsolete_total_obsolete_same_high)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 50;
    const size_t patch_1_high = 99;

    const size_t patch_2_low = 0;
    const size_t patch_2_high = 99;

    patch_overlap_base(patch_size,
            patch_1_low, patch_1_high,
            patch_2_low, patch_2_high);

    patch_obsolete_delete_check();
}

test(patch_obsolete_total_obsolete_same)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    const size_t patch_2_low = 0;
    const size_t patch_2_high = 99;

    patch_overlap_base(patch_size,
            patch_1_low, patch_1_high,
            patch_2_low, patch_2_high);

    patch_obsolete_delete_check();
}

test(patch_obsolete_delete_while_apply)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    const size_t patch_2_low = 0;
    const size_t patch_2_high = 99;

    char *patch_content = malloc(patch_size);

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[patch_1_low],
            (mpatch_addr_t)&patch_content[patch_1_high],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    mpatch_modify_region(patch,
            (mpatch_addr_t)&patch_content[patch_2_low],
            (mpatch_addr_t)&patch_content[patch_2_high],
            MPATCH_CONTINUOUS);

    // Commit a checkpoint (stage and commit patches)
    fake_checkpoint();

    // Commit the checkpoint for patch_2
    fake_checkpoint();

    // Apply and delete
    mpatch_apply_all(true);

    free(patch_content);

    /* Delete tests: */

    // During the apply, patch 1 should have been deleted

    // Get the newly created nvm patch
    mpatch_origin_t *origin = &mpatch_active_patch_origin[MPATCH_FIRST_ID];

    // Patch 1 should now be deleted
    mpatch_patch_t *nvm_patch_1_new = origin->patch_list->next; // old patch (1)
    assert_true(nvm_patch_1_new == NULL);
}


test(patch_obsolete_delete_while_apply_all_blocks)
{
    // Commit blocks untill the memory is full, but make them overlap
    // Applying the patches should only leave ONE patch, the last one

    const size_t n_patches = REQUIRED_BLISS_BLOCKS;

    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    char *patch_content = malloc(patch_size);

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[patch_1_low],
            (mpatch_addr_t)&patch_content[patch_1_high],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    // Apply patches untill the memory is full
    for (int i=0; i<n_patches; i++) {
        // Commit a checkpoint (stage and commit patches)
        fake_checkpoint();
    }

    // Apply and delete
    mpatch_apply_all(true);

    free(patch_content);

    /* Delete tests: */

    // During the apply, patch 1 should have been deleted

    // Get the newly created nvm patch
    mpatch_origin_t *origin = &mpatch_active_patch_origin[MPATCH_FIRST_ID];

    mpatch_patch_t *nvm_patch_first = origin->patch_list;
    assert_true(nvm_patch_first != NULL);

    // All but one patch should be deleted
    mpatch_patch_t *nvm_patch_second = nvm_patch_first->next;
    assert_true(nvm_patch_second == NULL);

}

test(patch_obsolete_skip_uncommitted)
{
    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    char *patch_content = malloc(patch_size);

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[patch_1_low],
            (mpatch_addr_t)&patch_content[patch_1_high],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    // Commit a checkpoint (stage and commit patches)
    fake_checkpoint();

    // Stage a checkpoint with the same range
    mpatch_stage();

    // Now the sweep-delte should NOT delete the original patch,
    // It should skip the staged patch
    mpatch_sweep_delete_obselete();

    // re-stage the patches
    mpatch_stage();

    free(patch_content);

    /* Delete tests: */

    // During the sweep-delete NO patch should have been deleted

    // Get the newly created nvm patch
    mpatch_origin_t *origin = &mpatch_active_patch_origin[MPATCH_FIRST_ID];

    mpatch_patch_t *nvm_patch_first = origin->patch_list;
    assert_true(nvm_patch_first != NULL);

    mpatch_patch_t *nvm_patch_second = nvm_patch_first->next;
    assert_true(nvm_patch_second != NULL);
}

test(patch_stage_until_no_more_mem_unrecoverable)
{
    // Commit blocks until the memory is full + 1
    // All patches are the same range, but staged and not committed

    // TODO: We can theoretically delete staged patches that are obsolete wrt
    // newer patches. If we do we need to rewrite the test to have a larger size
    // every commit. This feature might not be needed though, not sure.

    const size_t n_patches = REQUIRED_BLISS_BLOCKS+1; // One more than availible

    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    char *patch_content = malloc(patch_size);

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[patch_1_low],
            (mpatch_addr_t)&patch_content[patch_1_high],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    // Stage patches untill the memory is full
    // We cant use a checkpoint, as a stage then would automatically try to
    // delte patches.
    for (int i=0; i<n_patches-1; i++) {
        // Commit a checkpoint (stage and commit patches)
        assert_true(mpatch_stage() == true);
    }

    // The next stage should fail
    assert_true(mpatch_stage_noretry() == false);

    free(patch_content);
}

test(patch_stage_until_no_more_mem_recoverable)
{
    // Commit blocks until the memory is full + 1
    // All patches are the same range, staged and committed

    const size_t n_patches = REQUIRED_BLISS_BLOCKS+1; // One more than availible

    const size_t patch_size = 100;

    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    char *patch_content = malloc(patch_size);

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[patch_1_low],
            (mpatch_addr_t)&patch_content[patch_1_high],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    // Stage patches untill the memory is full
    // We cant use a checkpoint, as a stage then would automatically try to
    // delte patches.
    for (int i=0; i<n_patches-1; i++) {
        // Commit a checkpoint (stage and commit patches)
        fake_checkpoint();
    }

    // The next stage should succeed after applying
    // mpatch_sweep_delete_obselete()
    assert_true(mpatch_stage() == true);

    free(patch_content);
}

// TODO make a test for staging range [0,0], it's special

// TODO: Intermittency tests
// - logical clock
// - restoration pending patches
// - restoration origin
// - delete recover

test(restore_pending_patches)
{
    const size_t patch_size = 100;
    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    char *patch_content = malloc(patch_size);

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[patch_1_low],
            (mpatch_addr_t)&patch_content[patch_1_high],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    char *pending_patches_save = malloc(sizeof(mpatch_pending_patches));
    assert_true(pending_patches_save != NULL);

    // Save the state
    memcpy(pending_patches_save, mpatch_pending_patches, sizeof(mpatch_pending_patches));

    fake_checkpoint();

    // Compare the state (should be the same)
    assert_true(memcmp(pending_patches_save, mpatch_pending_patches, sizeof(mpatch_pending_patches)) == 0);

    // Modify the pending patches
    mpatch_modify_region(patch, 10, 20, MPATCH_CONTINUOUS);

#if 0
    mpatch_pending_patch_t *patch2 = mpatch_get(MPATCH_FIRST_ID);
    mpatch_new_region(patch2,
            (mpatch_addr_t)&patch_content[patch_1_low],
            (mpatch_addr_t)&patch_content[patch_1_high],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch2);
#endif

    // Compare the state (should differ)
    assert_true(memcmp(pending_patches_save, mpatch_pending_patches, sizeof(mpatch_pending_patches)) != 0);

    assert_true(del_modify_flag == 0);
    fake_powerfailure();

    // Compare the state (should be the same)
    assert_true(memcmp(pending_patches_save, mpatch_pending_patches, sizeof(mpatch_pending_patches)) == 0);
}

test(restore_origin)
{
    const size_t patch_size = 100;
    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    char *patch_content = malloc(patch_size);

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[patch_1_low],
            (mpatch_addr_t)&patch_content[patch_1_high],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    // Create patches
    mpatch_stage();

    // Save the current origin state
    mpatch_origin_t mpatch_origin_save = *mpatch_active_patch_origin;

    fake_checkpoint_nostage();

    // Should be the same
    assert_true(memcmp(&mpatch_origin_save, mpatch_active_patch_origin, sizeof(mpatch_origin_save)) == 0);

    fake_powerfailure();

    // Should be the same
    assert_true(memcmp(&mpatch_origin_save, mpatch_active_patch_origin, sizeof(mpatch_origin_save)) == 0);

    //
    // Fail during staging

    int free_blocks_before_stage = bliss_active_allocator->n_free_blocks;

    // Create patches
    mpatch_stage();

    int free_blocks_after_stage = bliss_active_allocator->n_free_blocks;
    assert_true(free_blocks_after_stage < free_blocks_before_stage);

    // Should be different
    assert_true(memcmp(&mpatch_origin_save, mpatch_active_patch_origin, sizeof(mpatch_origin_save)) != 0);

    fake_powerfailure();

    int free_blocks_after_recover = bliss_active_allocator->n_free_blocks;
    assert_true(free_blocks_after_recover == free_blocks_before_stage);

    // Should be the same
    assert_true(memcmp(&mpatch_origin_save, mpatch_active_patch_origin, sizeof(mpatch_origin_save)) == 0);

}

test(fail_during_patch_delete)
{
    const size_t patch_size = 100;
    const size_t patch_1_low = 0;
    const size_t patch_1_high = 99;

    char *patch_content = malloc(patch_size);

    mpatch_id_t id = MPATCH_FIRST_ID;
    mpatch_pending_patch_t *patch = mpatch_get(id);
    mpatch_new_region(patch,
            (mpatch_addr_t)&patch_content[patch_1_low],
            (mpatch_addr_t)&patch_content[patch_1_high],
            MPATCH_CONTINUOUS);
    mpatch_enable(patch);

    fake_checkpoint();

    mpatch_origin_t *origin;

    // Get the newly created nvm patch
    origin = &mpatch_active_patch_origin[MPATCH_FIRST_ID];
    mpatch_patch_t *first_patch = origin->patch_list;

    // Second checkpoint with the same range
    fake_checkpoint();

    // Get the second created nvm patch
    origin = &mpatch_active_patch_origin[MPATCH_FIRST_ID];
    mpatch_patch_t *second_patch = origin->patch_list;

    assert_true(second_patch->next == first_patch);

    // Now we fake a failed delete, by:
    // 1. Deleting the patch
    // 2. Setting the del_modify_flag to 1
    // 3. Messing up the next pointer of patch 1 to random data (should be NULL)
    // 4. Do a restore

    int free_blocks_before_del = bliss_active_allocator->n_free_blocks;

    //printf("N free blocks: %d\n", bliss_active_allocator->n_free_blocks);
    mpatch_sweep_delete_obselete();
    //printf("N free blocks: %d\n", bliss_active_allocator->n_free_blocks);

    assert_true(second_patch->next == NULL);

    int free_blocks_after_del = bliss_active_allocator->n_free_blocks;

    assert_true(free_blocks_after_del > free_blocks_before_del);

    del_modify_flag = 1;
    second_patch->next = (void *)42; // Scramble the next pointer to an error state

    fake_powerfailure_restore();

    //printf("N free blocks: %d\n", bliss_active_allocator->n_free_blocks);

    assert_true(bliss_active_allocator->n_free_blocks == free_blocks_after_del);

    // The patch should still be deleted, it was obsolete anyway
    assert_true(second_patch->next == NULL);
}

test(undo_staged_patches)
{
    int test_var = 0;
    mpatch_origin_t *origin;
    mpatch_patch_t *nvm_patch;

    mpatch_pending_patch_t pp;
    mpatch_new_region(&pp,
                      (mpatch_addr_t)&test_var,
                      (mpatch_addr_t)&mpatch_util_last_byte(test_var),
                      MPATCH_STANDALONE);
    test_var = 7;

    int free_blocks_before_stage = bliss_active_allocator->n_free_blocks;

    mpatch_stage_patch(MPATCH_GENERAL, &pp);

    int free_blocks_after_stage = bliss_active_allocator->n_free_blocks;

    assert_true(free_blocks_after_stage < free_blocks_before_stage);

    origin = &mpatch_active_patch_origin[MPATCH_GENERAL];
    nvm_patch = origin->patch_list;
    assert_true(nvm_patch != NULL);

    assert_true(test_var == 7);

    fake_powerfailure_restore();

    origin = &mpatch_active_patch_origin[MPATCH_GENERAL];
    nvm_patch = origin->patch_list;
    assert_true(nvm_patch == NULL);

    assert_true(test_var == 7);

    int free_blocks_after_uncommit = bliss_active_allocator->n_free_blocks;
    assert_true(free_blocks_after_uncommit > free_blocks_after_stage);
    assert_true(free_blocks_after_uncommit == free_blocks_before_stage);
}

test(patch_first_all_then_some)
{
    #define MEM_SAME() (memcmp(memory, memory_compare, memory_size) == 0)

    // 10 patches
    const size_t memory_size = 1000;
    const size_t patch_size = 100;
    const size_t n_patches = memory_size/patch_size;

    mpatch_pending_patch_t pp;
    uintptr_t mpatchregionstart;
    uintptr_t mpatchregionend;

    char *memory = malloc(memory_size);
    char *memory_compare = malloc(memory_size);

    assert_true(memory != NULL);
    assert_true(memory_compare != NULL);

    // Initial state
    for (int i=0; i<memory_size; i++) {
        memory[i] = (uint8_t)i;
    }

    memcpy(memory_compare, memory, memory_size);

    // Create all checkpoints for the initial memory state
    for (int i=0; i<n_patches; i++) {
        mpatchregionstart = memory + (i * patch_size);
        mpatchregionend = mpatchregionstart + (patch_size-1);

        mpatch_new_region(&pp,
                          (mpatch_addr_t)mpatchregionstart,
                          (mpatch_addr_t)mpatchregionend,
                          MPATCH_STANDALONE);
        mpatch_stage_patch_retry(MPATCH_GENERAL, &pp);
    }

    fake_checkpoint();

    // Scramble the memory
    memset(memory, 0, memory_size);

    // Memory should be different
    assert_false(MEM_SAME());

    // Restore
    fake_powerfailure_restore();

    // Memory should be the same
    assert_true(MEM_SAME());

    // Touch a regions and checkpoint only that one
    int region = 3;
    int region_location = 4;
    int touch_idx = patch_size*region+region_location;
    memory[touch_idx] += 5;

    // Memory should be different
    assert_false(MEM_SAME());

    // Checkpoint the region
    mpatchregionstart = memory + (region * patch_size);
    mpatchregionend = mpatchregionstart + (patch_size-1);
    mpatch_new_region(&pp,
                      (mpatch_addr_t)mpatchregionstart,
                      (mpatch_addr_t)mpatchregionend,
                      MPATCH_STANDALONE);
    mpatch_stage_patch_retry(MPATCH_GENERAL, &pp);

    // Save to compare
    memcpy(memory_compare, memory, memory_size);

    fake_checkpoint();

    // Restore
    fake_powerfailure_restore();

    // Memory should be the same
    assert_true(MEM_SAME());

    free(memory);
    free(memory_compare);
}

#define TOUCH_AND_PATCH_MEM_SIZE 1000
#define TOUCH_AND_PATCH_PATCH_SIZE 100
static void touch_and_patch(char *memory, size_t region, size_t location)
{
    const size_t memory_size = TOUCH_AND_PATCH_MEM_SIZE;
    const size_t patch_size = TOUCH_AND_PATCH_PATCH_SIZE;
    const size_t n_patches = memory_size/patch_size;

    mpatch_pending_patch_t pp;
    uintptr_t mpatchregionstart;
    uintptr_t mpatchregionend;

    // Touch a regions and checkpoint only that one
    int region_location = location%patch_size;

    int touch_idx = patch_size*region+region_location;
    //printf("Touch idx: %d\n", touch_idx);
    //printf("Touch addr: %p\n", &memory[touch_idx]);
    //printf("Old: %u\n", (unsigned char)memory[touch_idx]);
    memory[touch_idx] += 1;
    //printf("New: %u\n", (unsigned char)memory[touch_idx]);

    // Checkpoint the region
    mpatchregionstart = memory + (region * patch_size);
    mpatchregionend = mpatchregionstart + (patch_size-1);
    mpatch_new_region(&pp,
                      (mpatch_addr_t)mpatchregionstart,
                      (mpatch_addr_t)mpatchregionend,
                      MPATCH_STANDALONE);
    mpatch_stage_patch_retry(MPATCH_GENERAL, &pp);

}

static void find_offending_region(char *memory, char *memory_compare)
{
    const size_t memory_size = TOUCH_AND_PATCH_MEM_SIZE;
    const size_t patch_size = TOUCH_AND_PATCH_PATCH_SIZE;
    const size_t n_patches = memory_size/patch_size;

    // Initial state
    for (int i=0; i<memory_size; i++) {
        if (memory[i] != memory_compare[i]) {
            size_t region = i/patch_size;
            printf("Offending index: %d, addr: %p, region (%d): %p\n",
                   i,
                   &memory[i],
                   region,
                   &memory[region]
                  );
            printf("Expected: %d, got: %d\n",
                   (unsigned char)memory_compare[i],
                   (unsigned char)memory[i]);

        }
    }

}

test(patch_first_random_one_by_one)
{
    #define MEM_SAME() (memcmp(memory, memory_compare, memory_size) == 0)

    const size_t fuzz_tries = 10000;

    // 10 patches
    const size_t memory_size = TOUCH_AND_PATCH_MEM_SIZE;
    const size_t patch_size = TOUCH_AND_PATCH_PATCH_SIZE;
    const size_t n_patches = memory_size/patch_size;

    mpatch_pending_patch_t pp;
    uintptr_t mpatchregionstart;
    uintptr_t mpatchregionend;

    unsigned char *memory = malloc(memory_size);
    unsigned char *memory_compare = malloc(memory_size);

    assert_true(memory != NULL);
    assert_true(memory_compare != NULL);

    // Initial state
    for (int i=0; i<memory_size; i++) {
        memory[i] = (uint8_t)i;
    }

    memcpy(memory_compare, memory, memory_size);

    // Create all checkpoints for the initial memory state
    for (int i=0; i<n_patches; i++) {
        mpatchregionstart = memory + (i * patch_size);
        mpatchregionend = mpatchregionstart + (patch_size-1);

        mpatch_new_region(&pp,
                          (mpatch_addr_t)mpatchregionstart,
                          (mpatch_addr_t)mpatchregionend,
                          MPATCH_STANDALONE);
        mpatch_stage_patch_retry(MPATCH_GENERAL, &pp);
    }

    fake_checkpoint();

    // Do for a random number a random number of times
    for (int try=1; try<=fuzz_tries; try++) {
      size_t region = rand()%n_patches;
      size_t location = rand()%patch_size;
      //printf("Try: %d, region: %d, location: %d\n", try, region, location);

      // Touch regions
      touch_and_patch(memory, region, location);

      // Save to compare
      memcpy(memory_compare, memory, memory_size);
      fake_checkpoint();
      // Restore
      fake_powerfailure_restore();

      find_offending_region(memory, memory_compare);

      // Memory should be the same
      assert_true(MEM_SAME());
    }


    free(memory);
    free(memory_compare);
}

static void shuffle(size_t *array, size_t n)
{
    if (n > 1)
    {
        size_t i;
        for (i = 0; i < n - 1; i++)
        {
          size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
          int t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }
}

test(patch_first_random_multiple)
{
    #define MEM_SAME() (memcmp(memory, memory_compare, memory_size) == 0)

    const size_t fuzz_tries = 100000;

    // 10 patches
    const size_t memory_size = TOUCH_AND_PATCH_MEM_SIZE;
    const size_t patch_size = TOUCH_AND_PATCH_PATCH_SIZE;
    const size_t n_patches = memory_size/patch_size;

    size_t regions[n_patches];

    // Fill the region array
    for (int i=0; i<n_patches; i++) {
        regions[i] = i;
    }

    mpatch_pending_patch_t pp;
    uintptr_t mpatchregionstart;
    uintptr_t mpatchregionend;

    unsigned char *memory = malloc(memory_size);
    unsigned char *memory_compare = malloc(memory_size);

    assert_true(memory != NULL);
    assert_true(memory_compare != NULL);

    // Initial state
    for (int i=0; i<memory_size; i++) {
        memory[i] = (uint8_t)i;
    }

    memcpy(memory_compare, memory, memory_size);

    // Create all checkpoints for the initial memory state
    for (int i=0; i<n_patches; i++) {
        mpatchregionstart = memory + (i * patch_size);
        mpatchregionend = mpatchregionstart + (patch_size-1);

        mpatch_new_region(&pp,
                          (mpatch_addr_t)mpatchregionstart,
                          (mpatch_addr_t)mpatchregionend,
                          MPATCH_STANDALONE);
        mpatch_stage_patch_retry(MPATCH_GENERAL, &pp);
    }

    fake_checkpoint();

    // Do for a random number a random number of times
    for (int try=1; try<=fuzz_tries; try++) {
      //printf("Try: %d\n", try);

      // Scramble the region array
      shuffle(regions, sizeof(regions)/sizeof(regions[0]));
      //printf("Regions: ");
      //for (int i=0; i<sizeof(regions)/sizeof(regions[0]); i++) {
      //  printf("%d, ", regions[i]);
      //}
      //printf("\n");
      size_t touch_regions = rand()%(n_patches+1); // 0-all regions
      //printf("Touch regions: %d\n", touch_regions);

      for (size_t touch = 0; touch<touch_regions; touch++) {
        size_t region = regions[touch];
        size_t location = rand()%patch_size;
        //printf("\tregion: %d, location: %d\n", region, location);

        touch_and_patch(memory, region, location);
      }

      // Save to compare
      memcpy(memory_compare, memory, memory_size);
      fake_checkpoint();
      // Restore
      fake_powerfailure_restore();

      find_offending_region(memory, memory_compare);

      // Memory should be the same
      assert_true(MEM_SAME());
    }


    free(memory);
    free(memory_compare);
}

/*
* Register Tests
*/
#define mpatch_cmocka_unit_test(t_) cmocka_unit_test_setup_teardown(t_, setup, teardown)
int main(void)
{
    const struct CMUnitTest tests[] = {
        /* Allocation tests */
        mpatch_cmocka_unit_test(cmocka_setup_teardown),
        mpatch_cmocka_unit_test(get_pending_patch_from_id),
        mpatch_cmocka_unit_test(new_pending_region_cont),
        mpatch_cmocka_unit_test(new_region_already_pending_patch),
        mpatch_cmocka_unit_test(modify_region_subregion_already_pending_patch),
        mpatch_cmocka_unit_test(modify_region_largerregion_already_pending_patch),
        mpatch_cmocka_unit_test(modify_region_overlapregion_already_pending_patch),
        mpatch_cmocka_unit_test(modify_region_otherregion_already_pending_patch),
        mpatch_cmocka_unit_test(stage_single_new_region_cont),
        mpatch_cmocka_unit_test(commit_restore_single_new_region_cont),
        mpatch_cmocka_unit_test(commit_restore_single_new_region_cont_multiblock),
        mpatch_cmocka_unit_test(commit_restore_single_new_region_cont_multiblock_twice),
        mpatch_cmocka_unit_test(commit_restore_modify_region),
        mpatch_cmocka_unit_test(commit_disabled_region),
        mpatch_cmocka_unit_test(commit_singleshot),

        /* Intervaltree tests (overlapping regions) */
        mpatch_cmocka_unit_test(patch_overlap_subpatch_lower_same),
        mpatch_cmocka_unit_test(patch_overlap_subpatch_higher_same),
        mpatch_cmocka_unit_test(patch_overlap_subpatch_inside),
        mpatch_cmocka_unit_test(patch_overlap_total_overlap),
        mpatch_cmocka_unit_test(patch_overlap_modify_outside_higher),
        mpatch_cmocka_unit_test(patch_overlap_modify_outside_lower),

        /* Patch delete tests */
        mpatch_cmocka_unit_test(patch_obsolete_total_obsolete_inside),
        mpatch_cmocka_unit_test(patch_obsolete_total_obsolete_same_low),
        mpatch_cmocka_unit_test(patch_obsolete_total_obsolete_same_high),
        mpatch_cmocka_unit_test(patch_obsolete_total_obsolete_same),
        mpatch_cmocka_unit_test(patch_obsolete_delete_while_apply),
        mpatch_cmocka_unit_test(patch_obsolete_delete_while_apply_all_blocks),
        mpatch_cmocka_unit_test(patch_obsolete_skip_uncommitted),

        mpatch_cmocka_unit_test(patch_stage_until_no_more_mem_unrecoverable),
        mpatch_cmocka_unit_test(patch_stage_until_no_more_mem_recoverable),

        /* Intermittency tests */
        mpatch_cmocka_unit_test(restore_pending_patches),
        mpatch_cmocka_unit_test(restore_origin),
        mpatch_cmocka_unit_test(fail_during_patch_delete),
        mpatch_cmocka_unit_test(undo_staged_patches),

        mpatch_cmocka_unit_test(patch_first_all_then_some),
        mpatch_cmocka_unit_test(patch_first_random_one_by_one),
        mpatch_cmocka_unit_test(patch_first_random_multiple),

    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
