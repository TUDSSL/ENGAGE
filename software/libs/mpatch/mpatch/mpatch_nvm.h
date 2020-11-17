#ifndef MPATCH_NVM_H_
#define MPATCH_NVM_H_

#include "mpatch.h"

extern mpatch_lclock_t mpatch_lclock[2];

extern mpatch_origin_t mpatch_patch_origin[2][MPATCH_PENDING_SLOTS];

extern nvm mpatch_pending_patch_t mpatch_pending_patches_nvm[2][MPATCH_PENDING_SLOTS];

#endif /* MPATCH_NVM_H_ */
