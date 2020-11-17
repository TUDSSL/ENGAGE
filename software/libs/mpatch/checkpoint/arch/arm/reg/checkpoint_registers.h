#ifndef CHECKPOINT_REGISTERS_H_
#define CHECKPOINT_REGISTERS_H_

#include <stdlib.h>
#include "checkpoint_arch.h"
#include "checkpoint_selector.h"

#define CHECKPOINT_N_REGISTERS 17

// NVM Checkpoint
extern registers_t registers_checkpoint_nvm[2][CHECKPOINT_N_REGISTERS];

/* Special barrier instructions
 * http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0489c/CIHGHHIE.html
 */

extern volatile registers_t registers[CHECKPOINT_N_REGISTERS];
extern volatile registers_t checkpoint_svc_restore;
#define checkpoint_restored() checkpoint_svc_restore

// TODO Check for the correct placement of DSB, prob. needed for cache flush?
#define CP_SAVE_REGISTERS()     \
  do {                          \
    checkpoint_svc_restore = 0; \
    __asm__ volatile("SVC 42"); \
    /*__DSB();*/                \
    /*__ISB();*/                \
  } while (0)

#define CP_RESTORE_REGISTERS()  \
  do {                          \
    checkpoint_svc_restore = 1; \
    __asm__ volatile("SVC 43"); \
  } while (0)

__attribute__((always_inline))
static inline size_t checkpoint_registers(void) {
  CP_SAVE_REGISTERS();

  if (checkpoint_restored() == 0) {
    // Store the registers
    char *b_registers = (char *)registers;
    char *cp = (char *)registers_checkpoint_nvm[checkpoint_get_active_idx()];
    for (int i = 0; i < sizeof(registers); i++) {
      cp[i] = b_registers[i];
    }
  }

  return sizeof(registers);
}

void restore_registers(void);

#endif /* CHECKPOINT_REGISTERS_H_ */
