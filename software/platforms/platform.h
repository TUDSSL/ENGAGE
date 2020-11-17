/*
 * platform.h
 *
 *  Created on: Jan 11, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef PLATFORMS_PLATFORM_H_
#define PLATFORMS_PLATFORM_H_

#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define CATSTR(A, B) A B

#if HWREV == 1
#include "gby-v1-0.h"
#elif HWREV == 2
#include "gby-v2-0.h"
#else
#error "Please define a platform the hardware revision."
#endif

#include "emulator_settings.h"

#ifdef CHECKPOINT
#include "checkpoint_cfg.h"
#endif

#ifndef CHECKPOINT_EXCLUDE_DATA
#define CHECKPOINT_EXCLUDE_DATA
#endif

#ifndef CHECKPOINT_EXCLUDE_BSS
#define CHECKPOINT_EXCLUDE_BSS
#endif

#endif /* PLATFORMS_PLATFORM_H_ */
