#ifndef MPATCH_UTIL_H_
#define MPATCH_UTIL_H_

#include "mpatch.h"

extern mpatch_origin_t *mpatch_active_patch_origin;
extern mpatch_pending_patch_t mpatch_pending_patches[MPATCH_PENDING_SLOTS];

extern mpatch_patch_t *it_root;

extern volatile uint8_t del_modify_flag;
extern mpatch_patch_t **del_modify_patch;
extern mpatch_patch_t *del_modify_patch_new_next;
extern mpatch_patch_t *del_free_patch;

#endif /* MPATCH_UTIL_H_ */
