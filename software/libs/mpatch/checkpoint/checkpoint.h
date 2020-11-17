#ifndef MICROPY_INCLUDED_LIB_CHECKPOINT_CHECKPOINT_H_
#define MICROPY_INCLUDED_LIB_CHECKPOINT_CHECKPOINT_H_

#include <stdint.h>
#include <stdbool.h>

#include "checkpoint_arch.h"
#include "checkpoint_cfg.h"
#include "checkpoint_setup.h"

void checkpoint_restore(void);
int checkpoint(void);
int checkpoint_onetime_setup(void);

/* Restore management */
bool checkpoint_restore_available(void);
void checkpoint_restore_invalidate(void);
void checkpoint_restore_set_availible(void);

#endif // MICROPY_INCLUDED_LIB_CHECKPOINT_CHECKPOINT_H_
