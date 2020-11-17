#ifndef CHECKPOINT_ARCH_CFG_H_
#define CHECKPOINT_ARCH_CFG_H_

#include <stdint.h>

extern uint32_t _erestore_stack;

#define checkpoint_restore_stack_top (&_erestore_stack)

#endif /* CHECKPOINT_ARCH_CFG_H_ */
