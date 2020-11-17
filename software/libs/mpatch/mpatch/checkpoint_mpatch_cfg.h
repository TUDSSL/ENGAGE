#ifndef CHECKPOINT_MPATCH_CFG_H_
#define CHECKPOINT_MPATCH_CFG_H_

/* .data section to checkpoint the linkerscript */
extern uint32_t _sdata;
extern uint32_t _sdata_norestore;

#define checkpoint_mpatch_data_start    ((uintptr_t)(&_sdata))
#define checkpoint_mpatch_data_end      ((uintptr_t)(&_sdata_norestore))

#endif /* CHECKPOINT_MPATCH_CFG_H_ */
