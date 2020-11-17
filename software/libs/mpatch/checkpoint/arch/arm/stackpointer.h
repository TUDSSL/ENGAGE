#ifndef STACKPOINTER_H_
#define STACKPOINTER_H_

#include <stdint.h>
#include "checkpoint_arch.h"

/*
 * Stack pointer helper functions
 */
__attribute__((always_inline)) static inline char* stackpointer_get(void) {
  char* sp;

  __asm__ volatile("MOV %[stack_ptr], SP \n\t"
                   : [ stack_ptr ] "=r"(sp) /* output */
                   :                        /* input */
                   : "memory"               /* clobber */
  );

  return sp;
}

__attribute__((always_inline)) static inline void stackpointer_set(char* sp) {
  __asm__ volatile("MOV SP, %[stack_ptr] \n\t"
                   :                       /* output */
                   : [ stack_ptr ] "r"(sp) /* input */
                   : "memory"              /* clobber */
  );
}

#endif /* STACKPOINTER_H_ */
