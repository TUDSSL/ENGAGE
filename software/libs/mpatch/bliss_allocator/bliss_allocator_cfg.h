#ifndef BLISS_ALLOCATOR_CFG_H_
#define BLISS_ALLOCATOR_CFG_H_

#include <stdint.h>

//#define BLISS_CUSTOM_MEMORY

#include "nvm.h"

typedef uint8_t bliss_lclock_t;     // The logical clock type, required for double buffering
typedef uint16_t bliss_block_idx_t; // The block index size

#define BLISS_BLOCK_DATA_SIZE   600   // The number of data-bytes in a BLISS block
#define BLISS_FIRST_BLOCK_SKIP  64    // The number of bytes to skip in the first block

#ifndef BLISS_CUSTOM_MEMORY
extern uint32_t _sbliss; // Defined in linker script
extern uint32_t _ebliss; // Defined in linker script

#define BLISS_START_ADDRESS   ((uintptr_t)(&_sbliss))
#define BLISS_END_ADDRESS     ((uintptr_t)(&_ebliss))
#endif

/**
 * The memcpy that is used by bliss to copy blocks
 */
#include <string.h>
#define bliss_memcpy memcpy

#endif /* BLISS_ALLOCATOR_CFG_H_ */
