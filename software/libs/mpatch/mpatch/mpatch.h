#ifndef MPATCH_H_
#define MPATCH_H_

#include <stdbool.h>
#include "mpatch_cfg.h"

/**
 * MPatch address type
 */
typedef uintptr_t mpatch_addr_t;

/**
 * MPatch ID
 * The index in the pending patch list
 */
typedef enum mpatch_id {
    MPATCH_FIRST_ID = 0,

    MPATCH_GENERAL = 0,

    //MPATCH_C_STACK,
    //MPATCH_C_DATA,
    //MPATCH_C_BSS,

    MPATCH_PENDING_SLOTS // The last one
} mpatch_id_t;
#define MPATCH_LAST_ID MPATCH_PENDING_SLOTS

/*
 * MPatch configuration
 */
typedef enum mpatch_patch_type {
    MPATCH_TYPE_UNCHANGED = -1,

    MPATCH_STANDALONE = 0,
    MPATCH_CONTINUOUS = 0,
    MPATCH_SINGLESHOT,
} mpatch_patch_type_t;

/**
 * MPatch high and low address that make up a range to checkpoint
 */
typedef struct mpatch_range {
    mpatch_addr_t low;
    mpatch_addr_t high;
} mpatch_range_t;

/**
 * MPatch enable-disable type
 */
typedef uint8_t mpatch_patch_state_t;
#define MPATCH_PATCH_DISABLED 0
#define MPATCH_PATCH_ENABLED  1

/**
 * MPatch pending patch
 */
typedef struct mpatch_pending_patch {
    mpatch_patch_type_t type;           // MPatch patch type
    mpatch_range_t range;               // The MPatch range
    mpatch_range_t max_pending_range;   // Maximum range since the last checkpoint
    mpatch_range_t max_range;           // Maximum ever range of the patch, udated at commit
    mpatch_patch_state_t state;         // Patch state (enabled or disabled)
} mpatch_pending_patch_t;

/**
 * Mpatch patch list entry
 * A patch to be applied
 */
typedef struct mpatch_patch {
    struct mpatch_patch *next;      // The next patch in the list
    mpatch_lclock_t stage_clock;    // Logical clock when the patch was staged
    mpatch_range_t range;           // The range of the patch

    struct mpatch_patch *left, *right; // Interval tree support
    mpatch_addr_t max;

    char data[];                   // Pointer to the patch content
} mpatch_patch_t;

/**
 * MPatch patch list origin
 * Holds a pointer to the first patch in the patch chain
 * and the maximum range the patch chain will hold
 */
typedef struct mpatch_origin {
    mpatch_range_t max_range;   // Maximum ever range of the patch
    mpatch_patch_t *patch_list;   // Pointer to the start of the patch list
} mpatch_origin_t;


/******************************************************************************
 * MPatch API                                                                 *
 ******************************************************************************/

int mpatch_core_restore(void);
int mpatch_core_checkpoint(bool overwrite_restore);
size_t mpatch_core_post_checkpoint(void);

/**
 * Get a pending patch from its patch ID
 */
mpatch_pending_patch_t *mpatch_get(mpatch_id_t id);

/**
 * Create a new patch region
 * discards all data outside of this region
 */
void mpatch_new_region(mpatch_pending_patch_t *patch, mpatch_addr_t low, mpatch_addr_t high, mpatch_patch_type_t type);

/**
 * Modify a patch region
 * Extends the total region of a patch, but only this region is updated
 */
void mpatch_modify_region(mpatch_pending_patch_t *patch, mpatch_addr_t low, mpatch_addr_t high, mpatch_patch_type_t type);

/**
 * Enable the patch
 * If a pending patch is enabled it is created the next time a checkpoint is
 * created
 */
void mpatch_enable(mpatch_pending_patch_t *patch);

/**
 * Disable the patch
 * If a pending patch is enabled it is created the next time a checkpoint is
 * created
 */
void mpatch_disable(mpatch_pending_patch_t *patch);


/**
 * Stage all patches for commit
 * This creates the complete patch and adds it into non-volatile memory
 * A patch is fully commited when the logical clock is changed
 */
bool mpatch_stage_cond_retry(bool retry);

static inline bool mpatch_stage(void) {
    return mpatch_stage_cond_retry(true);
}

static inline bool mpatch_stage_noretry(void) {
    return mpatch_stage_cond_retry(false);
}


/**
 * Restore all patches
 */
void mpatch_restore_patches(void);

void mpatch_apply_all(bool delete_obsolete);

void mpatch_sweep_delete_obselete(void);


/**
 * Recover the mpatch chain if a power failure happens during a delete
 */
void mpatch_recover(void);

size_t mpatch_init(void);

mpatch_patch_t *mpatch_stage_patch(mpatch_id_t id, const mpatch_pending_patch_t *const patch);
mpatch_patch_t *mpatch_stage_patch_retry(mpatch_id_t id, const mpatch_pending_patch_t *const patch);

#define mpatch_util_last_byte(var_) (((char *)&var_)[sizeof(var_)-1])

#include "checkpoint_selector.h"
#define mpatch_active_lclock    mpatch_lclock[checkpoint_get_active_idx()]
#define mpatch_restore_lclock   mpatch_lclock[checkpoint_get_restore_idx()]

#endif /* MPATCH_H_ */
