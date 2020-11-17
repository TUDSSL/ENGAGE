/*
 * checkpoint_memtracker.c
 *
 *  Created on: Jan 23, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#include "checkpoint_memtracker.h"

#include <stdint.h>

#include "memtracker.h"
#include "z80_ub.h"

#ifdef CHECKPOINT
#include "mpatch.h"

#define MPATCH_CP_MEMTRACKER

#endif

size_t setup_memtracker(void) {
  initTracking(z80.memory);
  return 0;
}

/*
 * At the start make a checkpoint for all regions
 */
void checkpoint_memtracker_default(void) {
  for (uint32_t i = 0; i < NUM_MPU_TOTAL_REGIONS; i++) {
    uint32_t mpatchregionstart = startAddress + i * SUB_REGIONSIZE_BYTES;
    uint32_t mpatchregionend = mpatchregionstart + (SUB_REGIONSIZE_BYTES - 1);

#ifdef MPATCH_CP_MEMTRACKER
    // Create the patch
    mpatch_pending_patch_t pp;
    mpatch_new_region(&pp, (mpatch_addr_t)mpatchregionstart,
                      (mpatch_addr_t)mpatchregionend, MPATCH_STANDALONE);
    mpatch_stage_patch_retry(MPATCH_GENERAL, &pp);
#endif
  }
}

/*
 * Loop the regions and checkpoint the ones that have been written to
 */
size_t checkpoint_memtracker(void) {
  for (uint32_t i = 0; i < NUM_MPU_TOTAL_REGIONS; i++) {
    if (regionTracker[i]) {
      uint32_t mpatchregionstart = startAddress + i * SUB_REGIONSIZE_BYTES;
      uint32_t mpatchregionend = mpatchregionstart + (SUB_REGIONSIZE_BYTES - 1);

#ifdef MPATCH_CP_MEMTRACKER
      // Create the patch
      mpatch_pending_patch_t pp;
      mpatch_new_region(&pp, (mpatch_addr_t)mpatchregionstart,
                        (mpatch_addr_t)mpatchregionend, MPATCH_STANDALONE);
      mpatch_stage_patch_retry(MPATCH_GENERAL, &pp);
#endif
    }
  }

  return 0;
}

/*
 * Clear the tracked memory locations
 */
size_t post_checkpoint_memtracker(void) {
  // Enable all the regions again
  initTracking(z80.memory);

  return 0;
}
