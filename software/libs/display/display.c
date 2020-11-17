/*
 * display.c
 *
 *  Created on: Dec 30, 2019
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#include "display.h"

#include <stdbool.h>
#include <string.h>

#include "am_util_delay.h"
#include "platform.h"

typedef enum {
  WriteLine,
  WriteMultipleLines,
  Update,
  ClearAll,
  None
} CurrentTransaction;

CHECKPOINT_EXCLUDE_BSS uint32_t DMATCBBuffer[256];

CHECKPOINT_EXCLUDE_DATA
am_hal_iom_config_t DisplayConfigIOM = {
    .eInterfaceMode = AM_HAL_IOM_SPI_MODE,
    .ui32ClockFreq = AM_HAL_IOM_2MHZ,
    .eSpiMode = AM_HAL_IOM_SPI_MODE_0,
    .pNBTxnBuf = &DMATCBBuffer[0],
    .ui32NBTxnBufLength = sizeof(DMATCBBuffer) / 4};

DisplayStatus spiWrite(uint8_t* data, uint16_t size, bool blocking);

DisplayBuffer displayBuffer;
#if NUM_PIXELS == 1
DisplayBufferBW displayBufferBW;
#endif

CHECKPOINT_EXCLUDE_BSS
void* displaySpiHandle;

static const uint8_t reverseBitsTable[] =
    {  // Could be replaced by RBIT ARM instruction
        0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
        0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};

void populateAddressInBuffer() {
#if LCD_TYPE == LS
  for (uint8_t i = 0; i < NUM_LINES + 1; ++i) {
    displayBuffer.line[i].address = i;
  }
#elif LCD_TYPE == LPM
  for (uint32_t i = 0; i < NUM_LINES + 1; ++i) {
    displayBuffer.line[i].address =
        (reverseBitsTable[i & 0xF] << 4) | reverseBitsTable[i >> 4];
  }
#endif
#if NUM_PIXELS == 1
  for (uint32_t i = 0; i < NUM_LINES + 1; ++i) {
    displayBufferBW.line[i].address =
        (reverseBitsTable[i & 0xF] << 4) | reverseBitsTable[i >> 4];
  }
#endif
}

// writes a single line based on a buffer of a single line.
void displayWriteLineCpy(uint8_t line, const uint8_t* data) {
  Command command = {};
  command.mode = true;
#if LCD_TYPE == LPM
#if NUM_PIXELS == 1
  command.OneBitData = true;
#elif NUM_PIXELS == 4
  command.FourBitsData = true;
#endif
#endif
  memcpy(displayBuffer.line[line].data, data, LINE_SIZE_BYTES);
  displayBuffer.line[line].cmd = command;

  spiWrite((uint8_t*)&displayBuffer.line[line], sizeof(LineBuffer) + 2,
           DISPLAY_BLOCKING_WRITES);
}

// writes a single line based on a buffer of a single line.
void displayWriteLine(uint8_t line) {
  Command command = {};
  command.mode = true;
#if LCD_TYPE == LPM
#if NUM_PIXELS == 1
  command.OneBitData = true;
#elif NUM_PIXELS == 4
  command.FourBitsData = true;
#endif
#endif
  displayBuffer.line[line].cmd = command;

  spiWrite((uint8_t*)&displayBuffer.line[line], sizeof(LineBuffer) + 2,
           DISPLAY_BLOCKING_WRITES);
}

void displayWriteScreen() {
  Command command = {};
  command.mode = true;
#if LCD_TYPE == LPM
#if NUM_PIXELS == 1
  command.OneBitData = true;
#elif NUM_PIXELS == 4
  command.FourBitsData = true;
#endif
#endif
  displayBuffer.line[1].cmd = command;

  spiWrite((uint8_t*)&displayBuffer.line[1],
           (NUM_LINES * sizeof(LineBuffer)) + 2, DISPLAY_BLOCKING_WRITES);
}

// writes a bar on the screen in the bottom NUM_LINES -1 through NUM_LINES - 6
static const uint8_t clearPixel[] = {0xf0, 0x0f};
void displayWriteBar(uint8_t percentFilled) {
  uint8_t pixelColor = (percentFilled > 50) ? COLOR_WHITE : COLOR_RED;
  // convert to the bar size. 100/4 = 25, 25*7 = 175 so need  + 1 for full bar
  // also prevents totally empty bar.
  uint8_t barLength = ((percentFilled / 4) * 7) + 1;
  for (uint8_t line = NUM_LINES - 6; line < NUM_LINES; line++) {
    for (uint8_t pixel = 0; pixel < barLength; pixel++) {
      displayBuffer.line[line].data[pixel / 2] &= clearPixel[pixel % 2];
      displayBuffer.line[line].data[pixel / 2] |=
          ((uint8_t)pixelColor << (4 * (pixel % 2)));
    }
    for (uint8_t pixel = barLength; pixel < LINE_SIZE; pixel++) {
      displayBuffer.line[line].data[pixel / 2] &= clearPixel[pixel % 2];
      displayBuffer.line[line].data[pixel / 2] |=
          ((uint8_t)COLOR_BLACK << (4 * (pixel % 2)));
    }
  }
  displayWriteMultipleLines(NUM_LINES - 6, NUM_LINES - (NUM_LINES - 6));
}

#if NUM_PIXELS == 1
void displayWriteLineBW(uint8_t line) {
  Command command = {};
  command.mode = true;
#if LCD_TYPE == LPM
#if NUM_PIXELS == 1
  command.OneBitData = true;
#elif NUM_PIXELS == 4
  command.FourBitsData = true;
#endif
#endif
  displayBufferBW.line[line].cmd = command;
  state.currentCommand = WriteMultipleLines;
  state.startingLine = line;
  spiWrite((uint8_t*)&displayBufferBW.line[line], sizeof(LineBufferBW) + 2,
           DISPLAY_BLOCKING_WRITES);
}
#endif

// Requires full screen buffer
void displayWriteMultipleLines(uint8_t startLine, uint8_t numLines) {
  Command command = {};
  command.mode = true;
#if LCD_TYPE == LPM
#if NUM_PIXELS == 1
  command.OneBitData = true;
#elif NUM_PIXELS == 4
  command.FourBitsData = true;
#endif
#endif

  displayBuffer.line[startLine].cmd = command;

  spiWrite((uint8_t*)&displayBuffer.line[startLine],
           numLines * sizeof(LineBuffer) + 2, DISPLAY_BLOCKING_WRITES);
  // 2 byte overflow is guaranteed by DisplayBuffer::dummy
}

static void displayClearAll() {
  uint8_t spiBuffer[2];
  Command command = {};
  command.allClear = true;

  spiBuffer[0] = command.asUint8_t;

  spiWrite(spiBuffer, 2, true);
}

DisplayStatus spiWrite(uint8_t* data, uint16_t size, bool blocking) {
  am_hal_iom_transfer_t transaction = {};
  transaction.eDirection = AM_HAL_IOM_TX;
  transaction.ui32NumBytes = size;
  transaction.pui32TxBuffer = (uint32_t*)data;
  transaction.uPeerInfo.ui32SpiChipSelect = DISPLAY_SPI_CS_CHANNEL;
  transaction.ui8Priority = true;

  if (blocking) {
    if (am_hal_iom_blocking_transfer(displaySpiHandle, &transaction)) {
      return DISPLAY_ERROR;
    }
  } else {
    if (am_hal_iom_nonblocking_transfer(displaySpiHandle, &transaction, NULL,
                                        0)) {
      return DISPLAY_ERROR;
    }
  }

  return DISPLAY_SUCCESS;
}

static DisplayStatus spiInit() {
  am_hal_gpio_pinconfig(DISPLAY_CLK_PIN, DisplayPinConfigCLK);
  am_hal_gpio_pinconfig(DISPLAY_MOSI_PIN, DisplayPinConfigMOSI);
  am_hal_gpio_pinconfig(DISPLAY_CS_PIN, DisplayPinConfigCS);

  if (am_hal_iom_initialize(DISPLAY_SPI_IOM, &displaySpiHandle) ||
      am_hal_iom_power_ctrl(displaySpiHandle, AM_HAL_SYSCTRL_WAKE, false) ||
      am_hal_iom_configure(displaySpiHandle, &DisplayConfigIOM) ||
      am_hal_iom_enable(displaySpiHandle)) {
    return DISPLAY_ERROR;
  }
  uint32_t lsbFirst = true;
  am_hal_iom_control(displaySpiHandle, AM_HAL_IOM_REQ_SPI_LSB,
                     &lsbFirst);  // lsb first

  NVIC_EnableIRQ(IOMSTR1_IRQn);
  return DISPLAY_SUCCESS;
}

void am_iomaster1_isr(void) {
  uint32_t ui32Status;

  if (!am_hal_iom_interrupt_status_get(displaySpiHandle, true, &ui32Status)) {
    if (ui32Status) {
      am_hal_iom_interrupt_clear(displaySpiHandle, ui32Status);
      am_hal_iom_interrupt_service(displaySpiHandle, ui32Status);
    }
  }
}

// Cut power to the LCD
static void setDisplayPower(bool state) {
  if (state) {
    am_hal_gpio_pinconfig(DISPLAY_VCC_PIN, DisplayPinConfigVCC);
    am_hal_gpio_state_write(DISPLAY_VCC_PIN, AM_HAL_GPIO_OUTPUT_SET);
  } else {
    am_hal_gpio_pinconfig(DISPLAY_VCC_PIN, g_AM_HAL_GPIO_DISABLE);
  }
}

#define ComTimer 2
#define ComTimerClkSource AM_HAL_CTIMER_HFRC_12KHZ
#define PERIOD 60
#define NUM_PULSES_PERIOD ((12000 / PERIOD) - 1)
#define NUM_PULSES_PERIOD_ON_TIME NUM_PULSES_PERIOD >> 1

static void displayInvertComInit() {
  //
  // Configure the output pin.
  //
  am_hal_ctimer_output_config(ComTimer, AM_HAL_CTIMER_TIMERA,
                              DISPLAY_EXTCOM_PIN, AM_HAL_CTIMER_OUTPUT_NORMAL,
                              AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA);

  //
  // Configure a timer to drive the LED.
  //
  am_hal_ctimer_config_single(
      ComTimer, AM_HAL_CTIMER_TIMERA,
      (AM_HAL_CTIMER_FN_PWM_REPEAT | ComTimerClkSource |
       AM_HAL_CTIMER_INT_ENABLE | AM_HAL_CTIMER_PIN_INVERT));

  //
  // Set up initial timer periods.
  //
  am_hal_ctimer_period_set(ComTimer, AM_HAL_CTIMER_TIMERA, NUM_PULSES_PERIOD,
                           NUM_PULSES_PERIOD_ON_TIME);
}

static void setComTimer(bool state) {
  if (state) {
    am_hal_ctimer_start(ComTimer, AM_HAL_CTIMER_TIMERA);
  } else {
    am_hal_ctimer_stop(ComTimer, AM_HAL_CTIMER_TIMERA);
  }
}

// Can be called at any point but is required before screen writes are executed
void displaySetup() { populateAddressInBuffer(); }

void displayConfig() {
  am_hal_gpio_pinconfig(DISPLAY_EXTCOM_PIN, g_AM_HAL_GPIO_OUTPUT);
  am_hal_gpio_state_write(DISPLAY_EXTCOM_PIN, AM_HAL_GPIO_OUTPUT_CLEAR);

  am_hal_gpio_pinconfig(DISPLAY_DISP_PIN, g_AM_HAL_GPIO_OUTPUT);
  am_hal_gpio_state_write(DISPLAY_DISP_PIN, AM_HAL_GPIO_OUTPUT_CLEAR);

  setDisplayPower(true);

  spiInit();
  am_util_delay_ms(1);
  displayClearAll();

  am_hal_gpio_state_write(DISPLAY_DISP_PIN, AM_HAL_GPIO_OUTPUT_SET);
  am_util_delay_us(30);

  displayInvertComInit();
  setComTimer(true);
}

void displayShutdown() {
  am_hal_gpio_state_write(DISPLAY_DISP_PIN, AM_HAL_GPIO_OUTPUT_CLEAR);
  setComTimer(false);
  displayClearAll();
  am_util_delay_us(30);
  setDisplayPower(false);
}
