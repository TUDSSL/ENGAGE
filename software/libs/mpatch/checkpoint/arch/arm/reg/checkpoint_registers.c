#include <stdint.h>

#include "checkpoint.h"
#include "checkpoint_selector.h"
#include "checkpoint_util_mem.h"
#include "checkpoint_arch.h"
#include "checkpoint_registers.h"

CHECKPOINT_EXCLUDE_BSS volatile registers_t registers[CHECKPOINT_N_REGISTERS];
CHECKPOINT_EXCLUDE_DATA volatile uint32_t checkpoint_svc_restore = 0;

const volatile registers_t*const registers_top = &registers[CHECKPOINT_N_REGISTERS];  // one after the end


void restore_registers(void)
{
    char *cp_restore = (char *)registers_checkpoint_nvm[checkpoint_get_restore_idx()];
    checkpoint_memcpy((char *)registers, cp_restore, sizeof(registers));
    CP_RESTORE_REGISTERS();
    // never reached
}

