/*
 * emulator.h
 *
 *  Created on: Jan 23, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef LIBS_EMULATOR_EMULATOR_H_
#define LIBS_EMULATOR_EMULATOR_H_

#include "am_mcu_apollo.h"
#include "display.h"
#include "emulator_settings.h"

void emulatorConfigIO(void);
void emulatorSetup(void);
void emulatorRun(void);
void emulatorSetRomSize(uint32_t size);

#endif /* LIBS_EMULATOR_EMULATOR_H_ */
