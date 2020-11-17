/******************************************************************************
 * This file contains non-volatile memory data structures                     *
 ******************************************************************************/
#include "nvm.h"
#include "checkpoint_arch.h"
#include "checkpoint_registers.h"

nvm registers_t registers_checkpoint_nvm[2][CHECKPOINT_N_REGISTERS];
