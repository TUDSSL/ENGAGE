#ifndef CHECKPOINT_MPATCH_H_
#define CHECKPOINT_MPATCH_H_

#include "mpatch.h"
#include "checkpoint_mpatch_cfg.h"

size_t checkpoint_mpatch(void);
size_t restore_mpatch(void);
size_t setup_mpatch(void);

#define post_checkpoint_mpatch mpatch_core_post_checkpoint

#endif /* CHECKPOINT_MPATCH_H_ */
