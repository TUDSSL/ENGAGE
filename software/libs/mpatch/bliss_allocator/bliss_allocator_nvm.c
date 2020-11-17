#include "bliss_allocator.h"

/******************************************************************************
 * This file contains all non-volatile memory data structures used by BLISS   *
 ******************************************************************************/

/**
 * Global singleton that manages the allocator
 */
nvm bliss_allocator_t bliss_allocator_nvm[2]; // two entries for double buffering

