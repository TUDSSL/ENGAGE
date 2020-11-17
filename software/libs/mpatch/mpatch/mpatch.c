//#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "mpatch.h"
#include "mpatch_nvm.h"
#include "checkpoint_selector.h"
#include "checkpoint.h"
#include "barrier.h"

/**
 * Active patch list origin
 */
CHECKPOINT_EXCLUDE_BSS
mpatch_origin_t *mpatch_active_patch_origin;


/**
 * Pending patches
 */
CHECKPOINT_EXCLUDE_BSS
mpatch_pending_patch_t mpatch_pending_patches[MPATCH_PENDING_SLOTS];

CHECKPOINT_EXCLUDE_BSS
mpatch_patch_t *it_root;


nvm volatile uint8_t del_modify_flag;
nvm mpatch_patch_t **del_modify_patch;
nvm mpatch_patch_t *del_modify_patch_new_next;
nvm mpatch_patch_t *del_free_patch;


/**
 * Prototypes
 */
static void mpatch_delete_patch(mpatch_patch_t *nvm_patch, mpatch_patch_t **nvm_patch_modify_ptr);
static void mpatch_free_patch(mpatch_patch_t *nvm_patch);


#define MPATCH_ACTIVE_IDX()     (mpatch_active_lclock%2)
#define MPATCH_INACTIVE_IDX()   ((mpatch_active_lclock+1)%2)

/**
 * Debugging
 */
#define PRINT_LOG   (0)
#define PRINT_DEBUG (0)

#if PRINT_DEBUG
#define DEBUG_PRINT(...)    do {printf("[mpatch-dbg] "); printf(__VA_ARGS__);} while (0)
#else
#define DEBUG_PRINT(...)
#endif

#if PRINT_LOG
#define LOG_PRINT(...)      do {printf("[mpatch] "); printf(__VA_ARGS__);} while (0)
#else
#define LOG_PRINT(...)
#endif


size_t mpatch_init(void)
{
    del_modify_flag = 0;
    size_t blocks = bliss_init();

    mpatch_lclock[0] = 0;
    mpatch_lclock[1] = 0;
    memset(mpatch_patch_origin, 0, sizeof(mpatch_patch_origin));
    memset(mpatch_pending_patches_nvm, 0, sizeof(mpatch_pending_patches_nvm));

    mpatch_core_restore();

    return blocks;
}

/**
 * Initialize the active mpatch_origin, required for bouble buffering and
 * intermittent computing
 */
int mpatch_core_restore(void)
{
    mpatch_active_lclock = mpatch_restore_lclock + 1;

    mpatch_alloc_core_restore(mpatch_active_lclock);

    mpatch_origin_t *mpatch_inactive_patch_origin;
    mpatch_pending_patch_t *inactive_pending_patches_nvm;

    mpatch_active_patch_origin = mpatch_patch_origin[MPATCH_ACTIVE_IDX()];
    mpatch_inactive_patch_origin = mpatch_patch_origin[MPATCH_INACTIVE_IDX()];

    DEBUG_PRINT("Active patch origin: %d\n", (long)mpatch_active_patch_origin);
    DEBUG_PRINT("Inctive patch origin: %d\n", (long)mpatch_inactive_patch_origin);

    /* Copy the previous origin state to the active origin state */
    for (int i=0; i<MPATCH_PENDING_SLOTS; i++) {
        mpatch_active_patch_origin[i] = mpatch_inactive_patch_origin[i];
        DEBUG_PRINT("Active origin ptr: %p, patch_list: %p\n",
                    &mpatch_active_patch_origin[i], mpatch_active_patch_origin[i].patch_list);
    }

    /* Copy the previous pending patches state to the volatile pending patches
     * state
     */
    inactive_pending_patches_nvm = mpatch_pending_patches_nvm[MPATCH_INACTIVE_IDX()];
    for (int i=0; i<MPATCH_PENDING_SLOTS; i++) {
        mpatch_pending_patches[i] = inactive_pending_patches_nvm[i];
    }

    DEBUG_PRINT("MPatch restore lclock after core cp: %d\n", mpatch_restore_lclock);
    DEBUG_PRINT("MPatch active lclock after core cp: %d\n", mpatch_active_lclock);


    return 0;
}

/*
 * Overwriting the restore will commit the MPatch state even if the next global
 * checkpoint does not happen. This way we effectively rewrite histoty.
 *
 * This feature is required to create mpatch checkpoints without requiering
 * to checkpoint the complete system. This happens for example when we
 * want to perminently delete MPatch patches.
 */
int mpatch_core_checkpoint(bool overwrite_restore)
{
    DEBUG_PRINT("MPatch global lclock before core cp: %d\n", lclock);
    DEBUG_PRINT("MPatch restore lclock before core cp: %d\n", mpatch_restore_lclock);
    DEBUG_PRINT("MPatch active lclock before core cp: %d\n", mpatch_active_lclock);

    LOG_PRINT("Core checkpoint - overwrite restore: %s\n",
              (overwrite_restore) ? "true" : "false");

    /* Commit the allocator state */
    mpatch_alloc_core_checkpoint(mpatch_active_lclock);

    /* Commit the pending patches */
    mpatch_pending_patch_t *active_pending_patches_nvm;

    active_pending_patches_nvm = mpatch_pending_patches_nvm[MPATCH_ACTIVE_IDX()];
    for (int i=0; i<MPATCH_PENDING_SLOTS; i++) {
        active_pending_patches_nvm[i] = mpatch_pending_patches[i];
    }

    if (overwrite_restore) {
      /* Increase the logical clock of the mpatch restore */
      barrier;
      mpatch_restore_lclock += 1;
    }

    return 0;
}

/**
 * After a checkpoint the mpatch origin state needs to be copied to the now
 * active allocator state (to continue with it)
 */
size_t mpatch_core_post_checkpoint(void)
{
    mpatch_core_restore();
    return 0;
}



static inline mpatch_lclock_t mpatch_get_lclock(void)
{
    return lclock;
}


static inline void mpatch_update_type(mpatch_pending_patch_t *patch, mpatch_patch_type_t type)
{
    if (type == MPATCH_TYPE_UNCHANGED) {
        return;
    }

    patch->type = type;
}

static inline mpatch_range_t mpatch_max_range(mpatch_range_t *r1, mpatch_range_t *r2)
{
    mpatch_range_t max_range;

    max_range.high = (r1->high > r2->high) ? r1->high : r2->high;
    max_range.low =  (r1->low < r2->low) ? r1->low : r2->low;

    return max_range;
}


mpatch_pending_patch_t *mpatch_get(mpatch_id_t id)
{
    /* This should not happen, the index is out of range */
    //assert(id >= MPATCH_FIRST_ID && id <= MPATCH_LAST_ID);

    return &mpatch_pending_patches[id];
}

void mpatch_new_region(mpatch_pending_patch_t *patch, mpatch_addr_t low, mpatch_addr_t high, mpatch_patch_type_t type)
{
    /* Enter a critical section */
    mpatch_checkpoint_disable();

    patch->range.low = low;
    patch->range.high = high;

    // We create a new region, we don't care about any of the data outside
    // of this new region
    patch->max_pending_range = patch->range;
    patch->max_range = patch->range;

    // Update the type if required
    mpatch_update_type(patch, type);

    /* Exit a critical section */
    mpatch_checkpoint_enable();
}

void mpatch_modify_region(mpatch_pending_patch_t *patch, mpatch_addr_t low, mpatch_addr_t high, mpatch_patch_type_t type)
{
    /* Enter a critical section */
    mpatch_checkpoint_disable();

    mpatch_range_t new_range = {.low = low, .high = high};

    // We update the range, we keep the maximum range the pending patch was
    // until the next checkpoint in `max_pending_range`
    patch->max_pending_range = mpatch_max_range(&patch->max_pending_range, &new_range);
    patch->range = new_range;

    // Update the type if required
    mpatch_update_type(patch, type);

    /* Exit a critical section */
    mpatch_checkpoint_enable();
}

void mpatch_enable(mpatch_pending_patch_t *patch)
{
    // Should be no need to disable the checkpoints for this
    patch->state = MPATCH_PATCH_ENABLED;
}

void mpatch_disable(mpatch_pending_patch_t *patch)
{
    patch->state = MPATCH_PATCH_DISABLED;
}

/**
 * Add the patch to the front of the list
 */
static void mpatch_add_to_list(mpatch_origin_t *origin, mpatch_patch_t *nvm_patch)
{
    // Add the patch
    nvm_patch->next = origin->patch_list;
    origin->patch_list = nvm_patch;
}

static mpatch_origin_t *mpatch_get_origin(mpatch_id_t id)
{
    mpatch_origin_t *origin = &mpatch_active_patch_origin[id];
    return origin;
}

mpatch_patch_t *mpatch_stage_patch(mpatch_id_t id, const mpatch_pending_patch_t *const patch)
{
    mpatch_patch_t *nvm_patch;

    mpatch_addr_t low, high, size;

    low = patch->max_pending_range.low;
    high = patch->max_pending_range.high;
    size = high-low;

    // The range is [low,high] so the size should be increased by 1
    // except if both low and high are 0, this is a special case
    if (low != 0 || high != 0) {
        size += 1;
    }

    // Allocate a patch in non-volatile memory
    nvm_patch = (mpatch_patch_t *)mpatch_alloc(sizeof(mpatch_patch_t) + size);
    if (nvm_patch == NULL) {
        return NULL;
    }
    nvm_patch->next = NULL;

    // Write the data to the patch
    mpatch_store(nvm_patch->data, (char *)low, size);

    // Update the nvm patch meta-data
    nvm_patch->stage_clock = mpatch_get_lclock();
    nvm_patch->range = patch->max_pending_range;

    mpatch_origin_t *origin = mpatch_get_origin(id);
    mpatch_add_to_list(origin, nvm_patch);

    // Update the max_range of the nvm patches
    // This will discard any previous checkpointed data outside of the region
    origin->max_range = patch->max_range;

    LOG_PRINT("Staged patch ID: %d - ptr: %p range: [%lx,%lx]\n",
              nvm_patch->stage_clock,
              nvm_patch,
              nvm_patch->range.low,
              nvm_patch->range.high);

    return nvm_patch;
}

mpatch_patch_t *mpatch_stage_patch_retry(mpatch_id_t id, const mpatch_pending_patch_t *const patch)
{
    mpatch_patch_t *nvm_patch;

    nvm_patch = mpatch_stage_patch(id, patch);
    if (nvm_patch == NULL) {
            // This was already the second try
            // unrecoverable out-of-memory
            LOG_PRINT("Failed staging patch range: [%lx,%lx]\n",
                      patch->range.low,
                      patch->range.high
                      );
            LOG_PRINT("Attempting to sweep and delete obsolete patches and re-stage patches\n");
            mpatch_sweep_delete_obselete();

            // Retry
            nvm_patch = mpatch_stage_patch(id, patch);
            if (nvm_patch == NULL) {
                printf("! FAILED STAGING PATCH RETRY\n");
                printf("! UNRECOVERABLE OUT OF MEMORY\n");
                while (1) {}
            }
    }
    return nvm_patch;
}

bool mpatch_stage_cond_retry(bool retry)
{
    bool second_try = false;

    for (mpatch_id_t id=MPATCH_FIRST_ID; id<MPATCH_LAST_ID; id++) {
        mpatch_pending_patch_t *patch = mpatch_get(id);
        if (patch->state == MPATCH_PATCH_ENABLED) {
            if (mpatch_stage_patch(id, patch) == NULL) {
                if (retry == false || second_try == true) {
                    // This was already the second try
                    // unrecoverable out-of-memory
                    LOG_PRINT("!STAGE ATTEMPT FAILED - NO MORE MEMORY!\n");
                    return false;
                }

                // Stage failed
                LOG_PRINT("Failed staging patch\n");
                LOG_PRINT("Attempting to sweep and delete obsolete patches and re-stage patches\n");
                mpatch_sweep_delete_obselete();
                second_try = true;

                // Restart the staging of all patches
                id=MPATCH_FIRST_ID;
                continue;
            }
        }
    }

    // Update the pending patches, we only do this if staging of all patches
    // succeed.
    for (mpatch_id_t id=MPATCH_FIRST_ID; id<MPATCH_LAST_ID; id++) {
        mpatch_pending_patch_t *patch = mpatch_get(id);
        if (patch->state == MPATCH_PATCH_ENABLED) {
            if (patch->type == MPATCH_SINGLESHOT) {
                patch->state = MPATCH_PATCH_DISABLED;
            }
            // Set the max_pending_range to the current range for the next checkpoint
            patch->max_pending_range = patch->range;
        }
    }

    LOG_PRINT("All patches succesfully staged\n");
    return true;
}


/******************************************************************************
 * Applying the patches
 ******************************************************************************/

static mpatch_patch_t *intervaltree_insert_node(mpatch_patch_t *node, mpatch_patch_t *new_node)
{
    new_node->left = NULL;
    new_node->right = NULL;

    mpatch_addr_t low = new_node->range.low;
    mpatch_addr_t high = new_node->range.high;
    new_node->max = new_node->range.high;

    // Tree is empty, new node becomes node
    if (node == NULL) {
        //printf("ERROR: root is empy\n");
        return new_node;
    }

    // Get low value of interval at node
    mpatch_addr_t l = node->range.low;

    // If node's low value is smaller, then new interval goes to left subtree
    if (low < l) {
        node->left = intervaltree_insert_node(node->left, new_node);
    } else {
        node->right = intervaltree_insert_node(node->right, new_node);
    }

    // Update the max value of this ancestor if needed
    if (node->max < high) {
        node->max = high;
    }

    return node;
}

static bool intervaltree_overlap(mpatch_addr_t l1, mpatch_addr_t h1, mpatch_addr_t l2, mpatch_addr_t h2)
{
    DEBUG_PRINT("Compare [%lx,%lx] and [%lx,%lx]\n", l1, h1, l2, h2);
    if ((l1 <= h2) && (l2 <= h1)) {
        return true;
    }
    return false;
}

/* Find any overlap */
static mpatch_patch_t *intervaltree_overlap_search(mpatch_patch_t *node, mpatch_addr_t low, mpatch_addr_t high)
{
    if (node == NULL) {
        return NULL;
    }

    // If interval overlaps with node
    if (intervaltree_overlap(node->range.low, node->range.high, low, high)) {
        return node;
    }

    // If left child of node is present and max of left child is greater than
    // or equal to the given interval, then the search interval may overlap
    // with an interval in its left subtree
    if ((node->left != NULL) && (node->left->max >= low)) {
        return intervaltree_overlap_search(node->left, low, high);
    }

    // Otherwise the search interval can only overlap with the right subtree
    return intervaltree_overlap_search(node->right, low, high);
}


static void mpatch_write(mpatch_patch_t *mp_apply, mpatch_addr_t low, mpatch_addr_t high)
{
    size_t size = high-low+1;
    char *dst = (char *)low;

    LOG_PRINT("Writing patch ID:  %d - ptr: %p range: [%lx,%lx] with apply range: [%lx,%lx] (size: %lu)\n",
            mp_apply->stage_clock,
            mp_apply,
            (unsigned long)mp_apply->range.low,
            (unsigned long)mp_apply->range.high,
            (unsigned long)low, (unsigned long)high,
            (unsigned long)size);

    // Compute number of blocks to skip in the bliss_allocator list
    // to get to the data
    size_t offset = low - mp_apply->range.low;
    mpatch_extract_woffset(dst, mp_apply->data, offset, size);
}


static bool mpatch_apply(mpatch_patch_t *mp_apply, mpatch_addr_t low, mpatch_addr_t high, mpatch_range_t *applied, bool write)
{
    mpatch_patch_t *p_overlap;

    DEBUG_PRINT("Applying patch ID: %d [%lx,%lx]\n", mp_apply->stage_clock, low, high);

    DEBUG_PRINT("Search overlap low = %lx, high = %lx\n", low, high);
    p_overlap = intervaltree_overlap_search(it_root, low, high);
    if (p_overlap == NULL) {
        // If it's a write
        if (write) {
            /* No overlap, write the patch */
            mpatch_write(mp_apply, low, high); // Actually apply the patch (range)
        }
        /* The max applied range */
        *applied = mpatch_max_range(applied, &(mpatch_range_t){.low=low, .high=high});
    } else {
        /* There is an overlap, split and apply again */
        DEBUG_PRINT("Overlap with: [%lx,%lx]\n", p_overlap->range.low, p_overlap->range.high);
        if (low < p_overlap->range.low) {
            DEBUG_PRINT("Lower Sub apply ID: %d [%lx,%lx]\n", mp_apply->stage_clock, low, p_overlap->range.low-1);
            mpatch_apply(mp_apply, low, p_overlap->range.low-1, applied, write);
        }
        if (high > p_overlap->range.high) {
            DEBUG_PRINT("Higher Sub apply ID: %d [%lx,%lx]\n", mp_apply->stage_clock, p_overlap->range.high+1, high);
            mpatch_apply(mp_apply, p_overlap->range.high+1, high, applied, write);
        }
        if (low >= p_overlap->range.low && high <= p_overlap->range.high) {
            return false;
        }
    }
    return true;
}


// Apply all patches from a patch ID
static void mpatch_apply_patch_chain(mpatch_origin_t *origin, bool free_obsolete, bool delete_obsolete, bool write)
{
    mpatch_patch_t *nvm_patch, **nvm_patch_modify_ptr;

    /* Reset the intervaltree */
    it_root = NULL;

    nvm_patch = origin->patch_list;
    nvm_patch_modify_ptr = &origin->patch_list; // The pointer to update

    /* Skip the patches we added during this logical clock, as they are
     * not committed.
     * We skip patches until we find a committed patch, all later patches
     * are OK, even if they have the same clock
     * A restore should have already removed these entries, as they are invallid
     */
    mpatch_lclock_t local_lclock = mpatch_get_lclock();
    while (nvm_patch != NULL && nvm_patch->stage_clock == local_lclock) {
      DEBUG_PRINT("Skipping patch applied during this lclock (%d) - ptr: %p\n",
                  (int)local_lclock, nvm_patch);
      // Move on to the next (older) patch
      nvm_patch_modify_ptr = &nvm_patch->next;
      nvm_patch = nvm_patch->next;
    }

    while (nvm_patch) {
        bool deleted_patch = false;

        mpatch_addr_t low = nvm_patch->range.low;
        mpatch_addr_t high = nvm_patch->range.high;

        mpatch_range_t applied_range = {.low=0, .high=0};

        if (mpatch_apply(nvm_patch, low, high, &applied_range, write)) {
            /* Add the patch to the interval tree */
            DEBUG_PRINT("Inserting interval [%lx,%lx]\n", low, high);
            it_root = intervaltree_insert_node(it_root, nvm_patch);
        } else if(free_obsolete || delete_obsolete) {
            if (free_obsolete) {
                // The node was NOT applied, free
                mpatch_free_patch(nvm_patch);
            }
            if (delete_obsolete) {
                // The node was NOT applied, delete it
                mpatch_delete_patch(nvm_patch, nvm_patch_modify_ptr);
                deleted_patch = true;
            }
        }

        // If a patch is deleted, we don't update the modify pointer
        if (deleted_patch == false) {
            // Move on to the next (older) patch
            nvm_patch_modify_ptr = &nvm_patch->next;
        }

        nvm_patch = nvm_patch->next;
        DEBUG_PRINT("Next ptr = %p\n", nvm_patch);
    }
}

/* Apply all the patches */
void mpatch_apply_all(bool delete_obsolete)
{
    // Applying all patches
    for (mpatch_id_t id=MPATCH_FIRST_ID; id<MPATCH_LAST_ID; id++) {
        DEBUG_PRINT("Applying patch chain: %d, delete: %d\n", id, delete_obsolete);
        mpatch_origin_t *origin = mpatch_get_origin(id);
        mpatch_apply_patch_chain(origin, false, delete_obsolete, true);
    }
}

void mpatch_sweep_delete_obselete(void)
{
    // Free staged patches (core restore is faster)
    //mpatch_core_restore();

    // Sweep and delete all the patches
    DEBUG_PRINT("Sweep-free patch chain\n");
    for (mpatch_id_t id=MPATCH_FIRST_ID; id<MPATCH_LAST_ID; id++) {
        mpatch_origin_t *origin = mpatch_get_origin(id);
        mpatch_apply_patch_chain(origin, true, false, false);
    }

    mpatch_core_checkpoint(true);
    mpatch_core_post_checkpoint();

    DEBUG_PRINT("Sweep-delete patch chain\n");
    for (mpatch_id_t id=MPATCH_FIRST_ID; id<MPATCH_LAST_ID; id++) {
        mpatch_origin_t *origin = mpatch_get_origin(id);
        mpatch_apply_patch_chain(origin, false, true, false);
    }

}


/******************************************************************************
 * Deleting obsolete patches
 ******************************************************************************/
/* TODO: Smarter patch merging can be implemented to save space
 */

static void mpatch_free_patch(mpatch_patch_t *nvm_patch)
{
    LOG_PRINT("Freeing patch ID: %d - ptr: %p range: [%lx,%lx]\n",
              nvm_patch->stage_clock, nvm_patch,
              nvm_patch->range.low,
              nvm_patch->range.high);
    mpatch_free((bliss_list_t *)nvm_patch);
}


static inline void mpatch_prepare_nvm_modify_patch_next(mpatch_patch_t **modify, mpatch_patch_t *free_patch, mpatch_patch_t *new_next)
{
    del_modify_patch = modify;
    del_modify_patch_new_next = new_next;
    del_free_patch = free_patch;

    barrier; // Flush cache

    del_modify_flag = 1;
}

static inline void mpatch_perform_nvm_modify_patch_next(void)
{
    *del_modify_patch = del_modify_patch_new_next;
}

static inline void mpatch_commit_nvm_modify_patch_next(void)
{
    barrier; // Flush cache
    del_modify_flag = 0;
}

static int mpatch_apply_uncommitted_chain(mpatch_origin_t *origin, bool free_uncommitted, bool delete_uncommitted)
{
    mpatch_patch_t *nvm_patch, **nvm_patch_modify_ptr;
    bool deleted_patch = false;
    int modify = 0;

    nvm_patch = origin->patch_list;
    nvm_patch_modify_ptr = &origin->patch_list; // The pointer to update

    /* Skip the patches we added during this logical clock, as they are
     * not committed.
     * We skip patches until we find a committed patch, all later patches
     * are OK, even if they have the same clock
     * A restore should have already removed these entries, as they are invallid
     */
    mpatch_lclock_t local_lclock = mpatch_get_lclock();
    while (nvm_patch != NULL && nvm_patch->stage_clock == local_lclock) {
      deleted_patch = false;
      DEBUG_PRINT("Fee/Delete patch applied during this lclock (%d) - ptr: %p\n",
                  (int)local_lclock, nvm_patch);

      if (free_uncommitted) {
        mpatch_free_patch(nvm_patch);
        modify = 1;
      }
      if (delete_uncommitted) {
        mpatch_delete_patch(nvm_patch, nvm_patch_modify_ptr);
        deleted_patch = true;
        modify = 1;
      }

      // If a patch is deleted, we don't update the modify pointer
      if (deleted_patch == false) {
        // Move on to the next (older) patch
        nvm_patch_modify_ptr = &nvm_patch->next;
      }
      nvm_patch = nvm_patch->next;
    }

    return modify;
}

static void mpatch_sweep_delete_uncommitted(void)
{
    // Free staged patches (core restore is faster)
    //mpatch_core_restore();
    bool freed_patches = false;

    // Sweep and delete all the patches
    DEBUG_PRINT("Sweep-free uncommitted patch chain\n");
    for (mpatch_id_t id=MPATCH_FIRST_ID; id<MPATCH_LAST_ID; id++) {
        mpatch_origin_t *origin = mpatch_get_origin(id);
        freed_patches += mpatch_apply_uncommitted_chain(origin, true, false);
    }

    if (freed_patches) {
      mpatch_core_checkpoint(true);
      mpatch_core_post_checkpoint();

      DEBUG_PRINT("Sweep-delete uncommitted patch chain\n");
      for (mpatch_id_t id=MPATCH_FIRST_ID; id<MPATCH_LAST_ID; id++) {
        mpatch_origin_t *origin = mpatch_get_origin(id);
        mpatch_apply_uncommitted_chain(origin, false, true);
      }
    }
}

/*
 * If we fail during a patch delte we need to recover
 */
void mpatch_recover(void)
{
    if (del_modify_flag != 0) {
        // A power failure occured during a delete operation
        // we can assume modify_node and modify_node_new_next are correct,
        // otherwise modify_flag would not yet be set.
        mpatch_perform_nvm_modify_patch_next();

        // Commit the patch delete and free the patch
        mpatch_commit_nvm_modify_patch_next();
    }

    mpatch_sweep_delete_uncommitted();
}

/*
 * Delete a patch
 */
static void mpatch_delete_patch(mpatch_patch_t *nvm_patch, mpatch_patch_t **nvm_patch_modify_ptr)
{
    LOG_PRINT("Deleting patch ID: %d - ptr: %p, modify patch ptr: %p range: [%lx,%lx]\n",
            nvm_patch->stage_clock, nvm_patch, nvm_patch_modify_ptr,
            nvm_patch->range.low,
            nvm_patch->range.high);

    // NB. Required to be able to recover during a delete
    mpatch_prepare_nvm_modify_patch_next(nvm_patch_modify_ptr, nvm_patch, nvm_patch->next);
    mpatch_perform_nvm_modify_patch_next();
    // NB. Required to be able to recover during a delete
    mpatch_commit_nvm_modify_patch_next();
}
