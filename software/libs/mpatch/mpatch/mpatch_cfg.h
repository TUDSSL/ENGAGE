#ifndef MPATCH_CFG_H_
#define MPATCH_CFG_H_

#include <stdint.h>

typedef uint8_t mpatch_lclock_t;     // The logical clock type, required for double buffering

/**
 * The number of slots allocated for pending patches
 * The index corresponds to the patch ID
 */
//#define MPATCH_PENDING_SLOTS 10     // The number of pending slots that can be used

/**
 * Place variable in non-volatile memory
 */
#include "nvm.h"

/**
 * Checkpoint management for "critical" sections
 */
#define mpatch_checkpoint_disable()
#define mpatch_checkpoint_enable()

/**
 * Patch data allocation API
 */
#include "bliss_allocator.h"

#define mpatch_alloc            bliss_alloc
#define mpatch_free             bliss_free
#define mpatch_store            bliss_store
#define mpatch_extract          bliss_extract
#define mpatch_extract_woffset  bliss_extract_woffset

/**
 * Allocator intermittency handling calls
 */
#define mpatch_alloc_core_checkpoint      bliss_checkpoint
#define mpatch_alloc_core_post_checkpoint bliss_post_checkpoint
#define mpatch_alloc_core_restore         bliss_restore

#endif /* MPATCH_CFG_H_ */
