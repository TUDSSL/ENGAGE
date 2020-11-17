/*
 * buttons.c
 *
 *  Created on: Mar 7, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#include "buttons.h"

#include "emulator_settings.h"

#define BUTTON_A_MASK ((uint64_t)0x1 << BUTTON_A)
#define BUTTON_B_MASK ((uint64_t)0x1 << BUTTON_B)

#define BUTTON_UP_MASK ((uint64_t)0x1 << BUTTON_UP)
#define BUTTON_DOWN_MASK ((uint64_t)0x1 << BUTTON_DOWN)
#define BUTTON_RIGHT_MASK ((uint64_t)0x1 << BUTTON_RIGHT)
#define BUTTON_LEFT_MASK ((uint64_t)0x1 << BUTTON_LEFT)

#define BUTTON_START_MASK ((uint64_t)0x1 << BUTTON_START)
#define BUTTON_SELECT_MASK ((uint64_t)0x1 << BUTTON_SELECT)

void debounceTimerInit();

void am_gpio_isr(void) {
  uint64_t ui64Status;
  am_hal_gpio_interrupt_status_get(false, &ui64Status);
  am_hal_gpio_interrupt_clear(ui64Status);

  // A B
  if (ui64Status & BUTTON_A_MASK) {
    processPress(KEYCODE_BTN_A, AM_HAL_CTIMER_TIMERA);
    GB.key.code_btn = KEYCODE_BTN_A;
    return;
  }
  if (ui64Status & BUTTON_B_MASK) {
    processPress(KEYCODE_BTN_B, AM_HAL_CTIMER_TIMERA);
    GB.key.code_btn = KEYCODE_BTN_B;
    return;
  }

  // DPad
  if (ui64Status & BUTTON_UP_MASK) {
    processPress(KEYCODE_CURSOR_UP, AM_HAL_CTIMER_TIMERB);
    GB.key.code_cursor = KEYCODE_CURSOR_UP;
    return;
  }
  if (ui64Status & BUTTON_DOWN_MASK) {
    processPress(KEYCODE_CURSOR_DOWN, AM_HAL_CTIMER_TIMERB);
    GB.key.code_cursor = KEYCODE_CURSOR_DOWN;
    return;
  }
  if (ui64Status & BUTTON_LEFT_MASK) {
    processPress(KEYCODE_CURSOR_LEFT, AM_HAL_CTIMER_TIMERB);
    GB.key.code_cursor = KEYCODE_CURSOR_LEFT;
    return;
  }
  if (ui64Status & BUTTON_RIGHT_MASK) {
    processPress(KEYCODE_CURSOR_RIGHT, AM_HAL_CTIMER_TIMERB);
    GB.key.code_cursor = KEYCODE_CURSOR_RIGHT;
    return;
  }

  // Start Select
  if (ui64Status & BUTTON_START_MASK) {
    processPress(KEYCODE_BTN_START, AM_HAL_CTIMER_TIMERA);
    GB.key.code_btn = KEYCODE_BTN_START;
    return;
  }
  if (ui64Status & BUTTON_SELECT_MASK) {
    processPress(KEYCODE_BTN_SELECT, AM_HAL_CTIMER_TIMERA);
    GB.key.code_btn = KEYCODE_BTN_SELECT;
    return;
  }
}

const uint64_t GpioIntMask = BUTTON_A_MASK | BUTTON_B_MASK | BUTTON_UP_MASK |
                             BUTTON_DOWN_MASK | BUTTON_RIGHT_MASK |
                             BUTTON_LEFT_MASK | BUTTON_START_MASK |
                             BUTTON_SELECT_MASK;

const am_hal_gpio_pincfg_t g_GpioIOConfig = {
    .uFuncSel = 3,
    .eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_DISABLE,
    .eGPInput = AM_HAL_GPIO_PIN_INPUT_ENABLE,
    .eGPRdZero = AM_HAL_GPIO_PIN_RDZERO_READPIN,
    .eIntDir = AM_HAL_GPIO_PIN_INTDIR_HI2LO};

void buttonsConfig() {
  am_hal_gpio_pinconfig(BUTTON_A, g_GpioIOConfig);
  am_hal_gpio_pinconfig(BUTTON_B, g_GpioIOConfig);
  am_hal_gpio_pinconfig(BUTTON_UP, g_GpioIOConfig);
  am_hal_gpio_pinconfig(BUTTON_DOWN, g_GpioIOConfig);
  am_hal_gpio_pinconfig(BUTTON_RIGHT, g_GpioIOConfig);
  am_hal_gpio_pinconfig(BUTTON_LEFT, g_GpioIOConfig);
  am_hal_gpio_pinconfig(BUTTON_START, g_GpioIOConfig);
  am_hal_gpio_pinconfig(BUTTON_SELECT, g_GpioIOConfig);

  debounceTimerInit();

  am_hal_gpio_interrupt_clear(GpioIntMask);
  am_hal_gpio_interrupt_enable(GpioIntMask);

  NVIC_EnableIRQ(GPIO_IRQn);
  am_hal_interrupt_master_enable();
}

#define DebounceTimerClkSource AM_HAL_CTIMER_HFRC_12KHZ
#define BUTTON_NUM_PULSES_PERIOD ((BUTTON_ACTIVE_PERIOD * 12) - 1)

void debounceTimerInit() {
  am_hal_ctimer_config_single(DebounceTimer, AM_HAL_CTIMER_TIMERA,
                              (AM_HAL_CTIMER_FN_ONCE | DebounceTimerClkSource |
                               AM_HAL_CTIMER_INT_ENABLE));
  am_hal_ctimer_config_single(DebounceTimer, AM_HAL_CTIMER_TIMERB,
                              (AM_HAL_CTIMER_FN_ONCE | DebounceTimerClkSource |
                               AM_HAL_CTIMER_INT_ENABLE));

  am_hal_ctimer_period_set(DebounceTimer, AM_HAL_CTIMER_TIMERA,
                           BUTTON_NUM_PULSES_PERIOD, 0);

  am_hal_ctimer_period_set(DebounceTimer, AM_HAL_CTIMER_TIMERB,
                           BUTTON_NUM_PULSES_PERIOD, 0);

  am_hal_ctimer_int_enable(AM_HAL_CTIMER_INT_TIMERA7C0);
  am_hal_ctimer_int_enable(AM_HAL_CTIMER_INT_TIMERB7C0);

  NVIC_EnableIRQ(CTIMER_IRQn);
  am_hal_interrupt_master_enable();
}

void am_ctimer_isr(void) {
  uint32_t intStatus = am_hal_ctimer_int_status_get(true);
  am_hal_ctimer_int_clear(intStatus);

  if (intStatus & AM_HAL_CTIMER_INT_TIMERA7C0) {
    GB.key.code_btn = KEYCODE_BTN_NONE;
    am_hal_ctimer_clear(DebounceTimer, AM_HAL_CTIMER_TIMERA);
  }
  if (intStatus & AM_HAL_CTIMER_INT_TIMERB7C0) {
    GB.key.code_cursor = KEYCODE_CURSOR_NONE;
    am_hal_ctimer_clear(DebounceTimer, AM_HAL_CTIMER_TIMERB);
  }
}
