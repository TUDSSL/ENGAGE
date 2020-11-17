#ifndef CHECKPOINT_CONTENT_H_
#define CHECKPOINT_CONTENT_H_

#include <stdlib.h>

#include "checkpoint_bss.h"
#include "checkpoint_data.h"
#include "checkpoint_stack.h"
#include "checkpoint_registers.h"
#include "checkpoint_mpatch.h"


// TODO make this somewhat autogen

__attribute__((always_inline))
static inline void CHECKPOINT_SETUP_CONTENT(void) {
  //setup_mpatch();
}

/*
 * Actions to be performed for a checkpoint
 */
__attribute__((always_inline))
static inline void CHECKPOINT_CONTENT(void) {
  checkpoint_data();
  checkpoint_bss();
  checkpoint_stack();
  //checkpoint_mpatch();
  checkpoint_registers(); // MUST BE LAST
}

/*
 * Actions to be performed for a restore
 */
__attribute__((always_inline))
static inline void CHECKPOINT_RESTORE_CONTENT(void) {
  restore_data();
  restore_bss();
  restore_stack();
  //restore_mpatch();
  restore_registers(); // MUST BE LAST
}

/*
 * Actions to be performed after a succesfull checkpoint
 */
__attribute__((always_inline))
static inline void POST_CHECKPOINT_CONTENT(void) {
  //post_checkpoint_mpatch();
}

/*
 * Actions to be performed after a succesfull checkpoint OR restore
 */
__attribute__((always_inline))
static inline void POST_CHECKPOINT_AND_RESTORE_CONTENT(void) {
}



#endif /* CHECKPOINT_CONTENT_H_ */
