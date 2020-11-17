#ifndef CHECKPOINT_STACK_H_
#define CHECKPOINT_STACK_H_

#include <stdlib.h>
#include "checkpoint_stack_cfg.h"

extern char stack_checkpoint_nvm_0[CHECKPOINT_STACK_SIZE];
extern char stack_checkpoint_nvm_1[CHECKPOINT_STACK_SIZE];

size_t checkpoint_stack(void);
size_t restore_stack(void);

#endif /* CHECKPOINT_STACK_H_ */
