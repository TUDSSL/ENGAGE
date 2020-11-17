#ifndef CHECKPOINT_BSS_CFG_H_
#define CHECKPOINT_BSS_CFG_H_

#include <stdint.h>

/* .bss section to checkpoint the linkerscript */
extern uint32_t _ebss_norestore;
extern uint32_t _ebss;

#define checkpoint_bss_start    (&_ebss_norestore)
#define checkpoint_bss_end      (&_ebss)


/* .bss NVM checkpoint section in the linkerscript */
extern uint32_t _bss_checkpoint_0_start;
extern uint32_t _bss_checkpoint_1_start;
extern uint32_t _bss_checkpoint_1_end;

#define checkpoint_bss_0_start  (&_bss_checkpoint_0_start)
#define checkpoint_bss_0_end    (&_bss_checkpoint_1_start)

#define checkpoint_bss_1_start  (&_bss_checkpoint_1_start)
#define checkpoint_bss_1_end    (&_bss_checkpoint_1_end)

#endif /* CHECKPOINT_BSS_CFG_H_ */
