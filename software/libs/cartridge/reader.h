/*
 * reader.h
 *
 *  Created on: Sep 27, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef LIBS_CARTRIDGE_READER_H_
#define LIBS_CARTRIDGE_READER_H_

#include "am_mcu_apollo.h"
#include "cartridge.h"

extern uint32_t _s_data_gb, _e_data_gb;

#define GameRomStart ((uintptr_t)(&_s_data_gb))
#define GameRomEnd ((uintptr_t)(&_e_data_gb))

#define CartridgeStart 0x0100
#define CartridgeTitleAddress 0x0134
#define CartridgeTypeAddress 0x0147
#define CartridgeRomSizeAddress 0x0148
#define CartridgeRamSizeAddress 0x0149

#define CartridgeRamBankRegisterAddress 0x2100
#define CartridgeRamBankAddress 0x4000
#define CartridgeRomBankSize 0x4000

uint8_t checkSavedCartridge(const uint8_t* startOfRom);

void cartridgeConfig();

uint8_t cartridgeSanityCheck();
void cartridgeReadInfo();
void cartridgeReadRom();

uint32_t cartridgeGetSizeBytes();

#endif /* LIBS_CARTRIDGE_READER_H_ */
