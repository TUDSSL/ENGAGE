#ifndef CHECKPOINT_CFG_H_
#define CHECKPOINT_CFG_H_


#define CHECKPOINT_EXCLUDE_DATA __attribute__((section(".norestore_data")))
#define CHECKPOINT_EXCLUDE_BSS  __attribute__((section(".norestore_bss")))


#endif /* CHECKPOINT_CFG_H_ */
