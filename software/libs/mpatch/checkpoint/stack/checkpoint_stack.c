#include <stdlib.h>
#include "checkpoint_selector.h"

#include "stackpointer.h"

#include "checkpoint_util_mem.h"
#include "checkpoint_stack.h"

struct stack_cp {
    char *stack_pointer;
    char data[];
};

size_t stack_size(char *stack_ptr) {
  size_t stack_size = (uintptr_t)stack_top - (uintptr_t)stack_ptr;
  return stack_size;
}

static inline char *stack_get_active_checkpoint(void) {
    char *cp;

    if (checkpoint_get_active_idx() == 0)
        cp = stack_checkpoint_nvm_0;
    else
        cp = stack_checkpoint_nvm_1;

    return cp;
}


static inline char *stack_get_restore_checkpoint(void) {
    char *cp;

    if (checkpoint_get_restore_idx() == 0)
        cp = stack_checkpoint_nvm_0;
    else
        cp = stack_checkpoint_nvm_1;

    return cp;
}

/*
 * .stack checkpoint and restore
 */
size_t checkpoint_stack(void) {
  struct stack_cp *cp = (struct stack_cp *)stack_get_active_checkpoint();

  char* stack_ptr = (char *)stackpointer_get();
  size_t size = stack_size(stack_ptr);

  cp->stack_pointer = stack_ptr;

  checkpoint_mem(cp->data, stack_ptr, size);
  return size;
}

size_t restore_stack(void) {
  struct stack_cp *cp = (struct stack_cp *)stack_get_restore_checkpoint();

  char* stack_ptr = cp->stack_pointer;
  size_t size = stack_size(stack_ptr);

  restore_mem(stack_ptr, cp->data, size);
  return size;
}
