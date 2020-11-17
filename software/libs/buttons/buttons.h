/*
 * buttons.h
 *
 *  Created on: Mar 8, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef LIBS_BUTTONS_BUTTONS_H_
#define LIBS_BUTTONS_BUTTONS_H_

#include "gameboy_ub.h"

void buttonsConfig();

#define DebounceTimer 7

__attribute__((always_inline)) static inline void processPress(
    uint16_t activeButton, uint32_t timerSection) {
  if (GB.key.code_btn == activeButton) {
    am_hal_ctimer_clear(DebounceTimer, timerSection);
    am_hal_ctimer_start(DebounceTimer, timerSection);
  } else {
    am_hal_ctimer_start(DebounceTimer, timerSection);
  }
}

#endif /* LIBS_BUTTONS_BUTTONS_H_ */
