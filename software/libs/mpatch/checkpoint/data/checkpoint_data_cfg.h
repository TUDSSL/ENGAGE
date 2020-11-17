#ifndef CHECKPOINT_DATA_CFG_H_
#define CHECKPOINT_DATA_CFG_H_

#include <stdint.h>

/* .data section to checkpoint the linkerscript */
extern uint32_t _edata_norestore;
extern uint32_t _edata;

#define checkpoint_data_start    (&_edata_norestore)
#define checkpoint_data_end      (&_edata)


/* .data NVM checkpoint section in the linkerscript */
extern uint32_t _data_checkpoint_0_start;
extern uint32_t _data_checkpoint_1_start;
extern uint32_t _data_checkpoint_1_end;

#define checkpoint_data_0_start  (&_data_checkpoint_0_start)
#define checkpoint_data_0_end    (&_data_checkpoint_1_start)

#define checkpoint_data_1_start  (&_data_checkpoint_1_start)
#define checkpoint_data_1_end    (&_data_checkpoint_1_end)

#endif /* CHECKPOINT_DATA_CFG_H_ */
