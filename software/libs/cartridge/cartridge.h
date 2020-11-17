/*
 * cartridge.h
 *
 *  Created on: Jan 11, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef LIBS_CARTRIDGE_CARTRIDGE_H_
#define LIBS_CARTRIDGE_CARTRIDGE_H_

#include "am_mcu_apollo.h"

#define SX150X_Address 0x20
// SZ1503 device address

#define SX150X_RegDataB 0x00
// RegDataB Data register for Bank B I/O[15:8]
#define SX150X_RegDataA 0x01
// RegDataA Data register for Bank A I/O[7:0]
#define SX150X_RegDirB 0x02
// RegDirB Direction register for Bank B I/O[15:8]
#define SX150X_RegDirA 0x03
// RegDirA Direction register for Bank A I/O[7:0]
#define SX150X_RegPullUpB 0x04
// RegPullUpB Pull-up register for Bank B I/O[15:8]
#define SX150X_RegPullUpA 0x05
// RegPullUpA Pull-up register for Bank A I/O[7:0]
#define SX150X_RegPullDownB 0x06
// RegPullDownB Pull-down register for Bank B I/O[15:8]
#define SX150X_RegPullDownA 0x07
// RegPullDownA Pull-down register for Bank A I/O[7:0]
#define SX150X_RegInterruptMaskB 0x08
// RegInterruptMaskB Interrupt mask register for Bank B I/O[15:8] 1111 1111
#define SX150X_RegInterruptMaskA 0x09
// RegInterruptMaskA Interrupt mask register for Bank A I/O[7:0] 1111 1111
#define SX150X_RegSenseHighB 0x0A
// RegSenseHighB Sense register for I/O[15:12]
#define SX150X_RegSenseHighA 0x0B
// RegSenseHighA Sense register for I/O[7:4]
#define SX150X_RegSenseLowB 0x0C
// RegSenseLowB Sense register for I/O[11:8]
#define SX150X_RegSenseLowA 0x0D
// RegSenseLowA Sense register for I/O[3:0]
#define SX150X_RegInterruptSourceB 0x0E
// RegInterruptSourceB Interrupt source register for Bank B I/O[15:8]
#define SX150X_RegInterruptSourceA 0x0F
// RegInterruptSourceA Interrupt source register for Bank A I/O[7:0]
#define SX150X_RegEventStatusB 0x10
// RegEventStatusB Event status register for Bank B I/O[15:8]
#define SX150X_RegEventStatusA 0x11
// RegEventStatusA Event status register for Bank A I/O[7:0]
#define SX150X_RegPLDModeB 0x20
// RegPLDModeB PLD mode register for Bank B I/O[15:8]
#define SX150X_RegPLDModeA 0x21
// RegPLDModeA PLD mode register for Bank A I/O[7:0]
#define SX150X_RegPLDTable0B 0x22
// RegPLDTable0B PLD truth table 0 for Bank B I/O[15:8]
#define SX150X_RegPLDTable0A 0x23
// RegPLDTable0A PLD truth table 0 for Bank A I/O[7:0]
#define SX150X_RegPLDTable1B 0x24
// RegPLDTable1B PLD truth table 1 for Bank B I/O[15:8]
#define SX150X_RegPLDTable1A 0x25
// RegPLDTable1A PLD truth table 1 for Bank A I/O[7:0]
#define SX150X_RegPLDTable2B 0x26
// RegPLDTable2B PLD truth table 2 for Bank B I/O[15:8]
#define SX150X_RegPLDTable2A 0x27
// RegPLDTable2A PLD truth table 2 for Bank A I/O[7:0]
#define SX150X_RegPLDTable3B 0x28
// RegPLDTable3B PLD truth table 3 for Bank B I/O[15:8]
#define SX150X_RegPLDTable3A 0x29
// RegPLDTable3A PLD truth table 3 for Bank A I/O[7:0]
#define SX150X_RegPLDTable4B 0x2A
// RegPLDTable4B PLD truth table 4 for Bank B I/O[15:8]
#define SX150X_RegPLDTable4A 0x2B
// RegPLDTable4A PLD truth table 4 for Bank A I/O[7:0]
#define SX150X_RegAdvanced 0xAD
// RegAdvanced Advanced settings register

typedef union {
  struct {
    uint8_t CartridgeCS : 1;
    uint8_t CartridgeRDn : 1;
    uint8_t CartridgeWRn : 1;
    uint8_t CartridgeCLK : 1;
    uint8_t CartridgeRSTn : 1;
    uint8_t dummy : 3;
  };
  uint8_t asUint8_t;
} CartridgeControlSignals;

void cartridgeInterfaceConfig();
void cartridgeInterfaceWriteControl(CartridgeControlSignals state);
void cartridgeInterfaceWriteData(uint16_t address, uint8_t data);
void cartridgeInterfaceReadData(uint16_t address, uint8_t* data);

#endif /* LIBS_CARTRIDGE_CARTRIDGE_H_ */
