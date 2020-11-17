/*
 * stm32f7xx.h
 *
 *  File to bridge the gap between the emulator and the target platform.
 *
 *  Created on: Mar 1, 2020
 *
 */

#ifndef CONFIG_STM32F7XX_H_
#define CONFIG_STM32F7XX_H_

#include "am_util.h"
#include "platform.h"

typedef enum {
  COM_1 = 0,
  COM_6 = 1,
  COM_7 = 2
} UART_NAME_t;

typedef enum {
  NONE = 0,
  LFCR,
  CRLF,
  LF,
  CR
} UART_LASTBYTE_t;

#define EndLine "\n"

#define UB_Uart_SendString(COM, STRING, ENDING) \
  am_util_stdio_printf(CATSTR(STRING, EndLine))

// Dummy file

#endif /* CONFIG_STM32F7XX_H_ */
