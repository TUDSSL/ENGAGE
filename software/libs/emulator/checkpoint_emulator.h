#ifndef CHECKPOINT_EMULATOR_H_
#define CHECKPOINT_EMULATOR_H_

#include "emulator.h"

static inline size_t restore_emulator(void) {
  emulatorConfigIO();
  return 0;
}

#endif /* CHECKPOINT_EMULATOR_H_ */
