/******************************************************************************
 * This file contains non-volatile memory data structures                     *
 ******************************************************************************/

#include "nvm.h"
#include "checkpoint_stack_cfg.h"

__attribute__((aligned(__alignof__(char *))))
nvm char stack_checkpoint_nvm_0[CHECKPOINT_STACK_SIZE];

__attribute__((aligned(__alignof__(char *))))
nvm char stack_checkpoint_nvm_1[CHECKPOINT_STACK_SIZE];
