# BLISS: Block-List with Intermittent Systems Support Allocator

This document describes BLISS, a allocator that creates a link of fixed sized blocks and links these together. Essentially it creates a linked-list of fixed
sized blocks; these blocks can then be used to store in the data.

BLISS is created to support [MPatch](https://github.com/TUDSSL/ENGAGE/tree/master/software/libs/mpatch/README.md), an intermittent computing framework that allows for patch-based checkpoints to reduce the checkpoint size (and therefore time). Because purely dynamic allocation leads to fragmentation, which is undesirable in embedded systems and **especially** undesirable in intermittent systems, as they are designed to run for a long time.

**Important Note: BLISS pointers and data should not be modified directly and should only be used through the provided API.**

### BLISS API

BLISS is controlled through the following API calls.

```
/* Allocate a bliss list that can hold 'size' bytes of data */
bliss_list_t *bliss_alloc(size_t size);

/* Free a bliss list */
bliss_free(bliss_list_t *free);

/* Copy data into a bliss list */
bliss_store(bliss_list_t *bliss_list, char *src);

/* Extract data from a bliss list */
bliss_extract(char *dst, bliss_list_t *bliss_list);
```

### BLISS Configuration

These are the configuration parameters that can be configured in [`bliss_allocator_cfg.h`](bliss_allocator_cfg.h).

```
//#define BLISS_CUSTOM_MEMORY

typedef uint8_t bliss_lclock_t;     // The logical clock type, required for double buffering
typedef uint16_t bliss_block_idx_t; // The block index size

#define BLISS_BLOCK_DATA_SIZE       // The number of data-bytes in a BLISS block
#define BLISS_FIRST_BLOCK_SKIP      // The number of bytes to skip in the first block

#define BLISS_START_ADDRESS 0xDEAD  // The start address
#define BLISS_END_ADDRESS   0xDEAD  // The end address
```

If `BLISS_CUSTOM_MEMORY` is defined, a memory must be separately allocated for the BLISS memory pool. Otherwise the addresses defined in `BLISS_START_ADDRESS` and `BLISS_END_ADDRESS` are used to define the start and the size of the memory pool.

## BLISS Usage

The following steps are needed to use BLISS in your project.

1. Include the BLISS source files in your project.
1. Configure the parameters in [`bliss_allocator_cfg.h`](bliss_allocator_cfg.h).
1. Use BLISS using the API [described above](#BLISS-API).

## BLISS Internals

BLISS links a number of blocks together into a linked list. The allocation needs to be fast, have little data overhead and, most importantly, be safe for use in intermittent computing system (i.e. where data is double buffered).

BLISS links blocks together using their block index and creates and maintains the free list in a way that introduces little additional memory overhead, it has `O(n)` complexity for allocating a link of `n` blocks.

## Limitations and Implementation-Dictated Requirements

No direct access to the memory is allowed, all memory read and write operations must be performed trough the [provided API](#BLISS-API).

## Credit

The BLISS allocator is **heavily** inspired by the allocator described in the following paper.

```
@inproceedings{kenwright:memory-pool:comp-tools:2012,
    author = {Ben {Kenwright}},
    title = {Fast Efficient Fixed-Size Memory Pool: No Loops and No Overhead},
    year = {2012},
    booktitle = {Proc. Computation Tools},
    month = jul # " 22--27", 
    address = {Nice, France},
    publisher = {IARIA}
}
```

