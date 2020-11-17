/*
 * checkpoint_memtracker.h
 *
 *  Created on: Jan 23, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef CHECKPOINT_MEMTRACKER_H_
#define CHECKPOINT_MEMTRACKER_H_

#include <stdlib.h>

size_t setup_memtracker(void);
size_t checkpoint_memtracker(void);

/* Restore is handled by MPatch */
// size_t restore_memtracker(void);
#define restore_memtracker()

size_t post_checkpoint_memtracker(void);

void checkpoint_memtracker_default(void);

#endif /* CHECKPOINT_MEMTRACKER_H_ */
