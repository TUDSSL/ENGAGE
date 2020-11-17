#ifndef CHECKPOINT_SETUP_RESTORE_H_
#define CHECKPOINT_SETUO_RESTORE_H_

#include "checkpoint_arch_cfg.h"
#include "stackpointer.h"

// Move the spackpointer to the safe stack where the restore code is executed
__attribute__((always_inline))
static inline void checkpoint_setup_restore(void)
{
    stackpointer_set((char *)checkpoint_restore_stack_top);
}


#endif /* CHECKPOINT_SETUP_RESTORE_H_ */
