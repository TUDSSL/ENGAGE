/*
 * emulator.c
 *
 *  Created on: Jan 23, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#include "emulator.h"

#include "am_util.h"
#include "buttons.h"
#include "gameboy_ub.h"
#include "jit_checkpoint.h"
#include "reader.h"

#ifdef CHECKPOINT
#include "checkpoint.h"
#endif

UB_GB_File Stored_ROM = {
    (uint8_t*)GameRomStart,  // pointer
    0,                       // size
};

void emulatorSetRomSize(uint32_t size) {
  Stored_ROM.size = size;
  Stored_ROM.table = (uint8_t*)GameRomStart;
}

void emulatorConfigIO(void) {
  displayConfig();
  buttonsConfig();
  jit_setup();
}

void emulatorSetup() {
  displaySetup();

  // init gameboy
  gameboy_init();

  if (Stored_ROM.size != 0) {
    gameboy_boot_flash(0);
  } else {
    am_util_stdio_printf(
        "Please hold down START and A whilst "
        "powering the device with a cartridge\n");
    while (1) {
      ;
    }
  }
}

#if LCD_TYPE == LS
// No longer supported
#elif LCD_TYPE == LPM
#if NUM_PIXELS == 1
extern DisplayBufferBW displayBufferBW;
void updateLineBW(uint8_t line) {
  for (uint16_t pixel = 0; pixel < LCD_MAXX; ++pixel) {
    uint8_t pixelValue =
        (displayBuffer.line[line].data[pixel / 2] >> (4 * (pixel % 2)));
    pixelValue &= 0xf;  // remove the other pixel for even numbers
    // counting down from 7 to 0, msbit is the first pixel
    if ((pixelValue == BG_COLOR_TRANSPARENT) ||
        (pixelValue == BG_COLOR_LIGHTGRAY)) {
      displayBufferBW.line[line].data[(pixel / 8)] |= (uint8_t)1
                                                      << ((pixel % 8));
    } else {
      displayBufferBW.line[line].data[(pixel / 8)] &=
          ~((uint8_t)1 << ((pixel % 8)));
    }
  }
  displayWriteLineBW(line);
}
#endif
#endif

CHECKPOINT_EXCLUDE_BSS
uint8_t jitCheckpointcount = 0;

void emulatorRun() {
  while (true) {
    gameboy_single_step();
#ifdef CHECKPOINT
    if (jit_checkpoint) {
      if (jitCheckpointcount == 0) {
        jitCheckpointcount = 5;
        checkpoint();
      } else {
        jitCheckpointcount--;
      }

      jit_checkpoint = 0;
    }
#endif
  }
}
