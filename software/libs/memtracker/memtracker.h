/*
 * memtracker.h
 *
 *  Created on: Mar 31, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef LIBS_MEMTRACKER_MEMTRACKER_H_
#define LIBS_MEMTRACKER_MEMTRACKER_H_

#include "am_mcu_apollo.h"
#include "emulator_settings.h"

#define NUM_MPU_REGIONS 8
#define NUM_MPU_SUB_REGIONS 8
#define NUM_MPU_TOTAL_REGIONS NUM_MPU_REGIONS* NUM_MPU_SUB_REGIONS

#define REGIONSIZE ARM_MPU_REGION_SIZE_4KB  // 4 * 8 = 32k
#define REGIONMASK ((2UL << REGIONSIZE) - 1)
#define SUBREGIONSIZE (REGIONSIZE - 3)
#define SUBREGIONMASK ((2UL << (SUBREGIONSIZE)) - 1)

#define REGIONSIZE_BYTES (2UL << REGIONSIZE)
#define SUB_REGIONSIZE_BYTES (2UL << SUBREGIONSIZE)

extern uint32_t startAddress;
extern uint32_t regionTracker[NUM_MPU_TOTAL_REGIONS];

void initTracking(uint8_t* z80RamPtr);

#endif /* LIBS_MEMTRACKER_MEMTRACKER_H_ */
