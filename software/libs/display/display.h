/*
 * display.h
 *
 *  Created on: Jan 6, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef LIBS_DISPLAY_DISPLAY_H_
#define LIBS_DISPLAY_DISPLAY_H_

#include "am_mcu_apollo.h"
#include "emulator_settings.h"

#define LS 1
#define LPM 2

// BGR not RGB -> red = 0b001
#define COLOR_RED 0b001
#define COLOR_GREEN 0b010
#define COLOR_BLUE 0b100
#define COLOR_CYAN 0b110
#define COLOR_YELLOW 0b011
#define COLOR_PURPLE 0b101
#define COLOR_WHITE 0b111
#define COLOR_BLACK 0b000

#ifdef LCD_COLOR
#define NUM_PIXELS 4
#else
#define NUM_PIXELS 1
#endif

#define LINE_SIZE_COLOR_BYTES LINE_SIZE * 4 / 8
#if LCD_TYPE == LS
#define NUM_LINES 168
#define LINE_SIZE 144
#define LINE_SIZE_BYTES LINE_SIZE* NUM_PIXELS / 8
#elif LCD_TYPE == LPM
#define NUM_LINES 176
#define LINE_SIZE 176
#define LINE_SIZE_BYTES LINE_SIZE* NUM_PIXELS / 8
#endif

#ifdef BLOCKING_WRITES
#define DISPLAY_BLOCKING_WRITES 1
#else
#define DISPLAY_BLOCKING_WRITES 0
#endif

typedef enum { DISPLAY_SUCCESS, DISPLAY_ERROR } DisplayStatus;

typedef union {
  struct {
    uint8_t mode : 1;
    uint8_t frameInv : 1;
    uint8_t allClear : 1;
    uint8_t FourBitsData : 1;  // R/G/B/D or when both one and four are low then
                               // 3 bit R/G/B, only for LPM013M126
    uint8_t OneBitData : 1;  // B/W or when both one and four are low then 3 bit
                             // R/G/B, only for LPM013M126
    uint8_t dummy : 3;
  };
  uint8_t asUint8_t;
} Command;

typedef struct {
  Command cmd;
  uint8_t address;
  uint8_t data[LINE_SIZE_COLOR_BYTES];
} LineBuffer;

typedef struct {
  LineBuffer line[NUM_LINES + 1];
  uint8_t dummy[2];
} DisplayBuffer;

#if NUM_PIXELS == 1
typedef struct {
  Command cmd;
  uint8_t address;
  uint8_t data[LINE_SIZE_BYTES];
} LineBufferBW;

typedef struct {
  LineBufferBW line[NUM_LINES + 1];
  uint8_t dummy[2];
} DisplayBufferBW;
#endif

void displaySetup();
void displayConfig();
void displayShutdown();

void displayWriteLine(uint8_t line);
void displayWriteScreen();

void displayWriteBar(uint8_t percentFilled);
void displayWriteMultipleLines(uint8_t startLine, uint8_t numLines);
void displayWriteLineCpy(uint8_t line, const uint8_t* data);

#if NUM_PIXELS == 1
void displayWriteLineBW(uint8_t line);
#endif

#endif /* LIBS_DISPLAY_DISPLAY_H_ */
