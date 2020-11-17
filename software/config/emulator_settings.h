/*
 * emulatorsettings.h
 *
 *  Created on: Apr 26, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef CONFIG_EMULATORSETTINGS_H_
#define CONFIG_EMULATORSETTINGS_H_

#define SUPPORTED_MBC_VERSION 1  // [0=w/o MBC, 1=MBC1]

//--------------------------------------------------------------
// Opcode GOTO
//-------------------------------------------------------------
#define OPCODE_GOTO 1

// Define JIT threshold in mV
// System turns off at 3.4V and when measuring has a 1/3 divider so
// always divide by 3!
//
// i.e. target shut off = 3.6
// 3.6 / 3 * 1000 (to mV) = 1200
#define jitTriggermV 3500

#define BUTTON_ACTIVE_PERIOD 300  // in ms

//#define ENABLE_ENERGY_BAR // enable to render a battery bar on the screen.

#define BW_THRESHOLD 0xf000

#define COLOR_LIGHT 0b111
#define COLOR_LIGHTGRAY 0b011
#define COLOR_GRAY 0b001
#define COLOR_BLACK 0b000

#define BG_COLOR_TRANSPARENT 0b1111
#define BG_COLOR_LIGHTGRAY 0b1011
#define BG_COLOR_GRAY 0b1001
#define BG_COLOR_BLACK 0b1000

#define LCD_TYPE LPM  // LS013B7DH05 or LPM013M126
#define LCD_COLOR     // Color screen or comment for b/w

//#define BLOCKING_WRITES // Uncomment for blocking writes

//#define TRACKING_COUNT_WRITES // Enable counting writes in z80 memory

#endif /* CONFIG_EMULATORSETTINGS_H_ */
