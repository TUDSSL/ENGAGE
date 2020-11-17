#include "nvm.h"
#include "mpatch.h"

/******************************************************************************
 * This file contains all non-volatile memory data structures used by MPatch  *
 ******************************************************************************/

/**
 * Logical clock, used to double buffer
 */
nvm mpatch_lclock_t mpatch_lclock[2];

/**
 * Per MPatch slot a pointer to the first patch to apply
 * Double buffered
 */
nvm mpatch_origin_t mpatch_patch_origin[2][MPATCH_PENDING_SLOTS];

/**
 * Checkpoint location for the pending patches
 * Double buffered
 */
nvm mpatch_pending_patch_t mpatch_pending_patches_nvm[2][MPATCH_PENDING_SLOTS];
