/*
 * emulator.c
 *
 *  Created on: Jan 23, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#include "am_mcu_apollo.h"
#include "am_util.h"
#include "checkpoint.h"
#include "checkpoint_memtracker.h"
#include "emulator.h"
#include "emulator_settings.h"
#include "fram.h"
#include "mpatch.h"
#include "mspi.h"
#include "platform.h"
#include "reader.h"
#include "z80_ub.h"

#define MSPI_TEST_MODULE 0

#if defined(AM_PART_APOLLO3)
#define MSPI_XIP_BASE_ADDRESS 0x04000000
#define MSPI_XIPMM_BASE_ADDRESS 0x51000000
#define MSPI_XIPMM_SIZE 0x100000
#elif defined(AM_PART_APOLLO3P)
#define MSPI_XIP_BASE_ADDRESS 0x02000000
#endif

CHECKPOINT_EXCLUDE_BSS uint32_t DMATCBBuffer_FRAM[2560];

const am_hal_mspi_dev_config_t MSPI_Flash_Serial_CE0_MSPIConfig = {
    .eSpiMode = AM_HAL_MSPI_SPI_MODE_0,
    .eClockFreq = AM_HAL_MSPI_CLK_24MHZ,
    .ui8TurnAround = 0,
    .eAddrCfg = AM_HAL_MSPI_ADDR_3_BYTE,
    .eInstrCfg = AM_HAL_MSPI_INSTR_1_BYTE,
    .eDeviceConfig = AM_HAL_MSPI_FLASH_SERIAL_CE0,
    .bSeparateIO = true,
    .bSendInstr = true,
    .bSendAddr = true,
    .bTurnaround = true,
    .ui8ReadInstr = FRAM_READ_MEM_CODE,
    .ui8WriteInstr = FRAM_WRITE_MEM_CODE,
    .ui32TCBSize = (sizeof(DMATCBBuffer_FRAM) / sizeof(uint32_t)),
    .pTCB = DMATCBBuffer_FRAM,
    .scramblingStartAddr = 0,
    .scramblingEndAddr = 0,
};

#ifdef CHECKPOINT
extern void startup_clear_data(void);
extern void startup_clear_bss(void);
#endif

int init(void) {
  uint32_t ui32Status;
  void* pHandle = NULL;

  //
  // Set the clock frequency.
  //
  am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, 0);

  //
  // Set the default cache configuration
  //
  am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
  am_hal_cachectrl_enable();
  // am_hal_cachectrl_disable();
  //
  // Initialize for low power in the power control block
  //
  am_hal_pwrctrl_low_power_init();

  //
  // Disable the RTC.
  //
  am_hal_rtc_osc_disable();

  // uncomment for debug output
  //  am_hal_tpiu_config_t TPIUcfg;
  //  //
  //  // Set the global print interface.
  //  //
  //
  //  //
  //  // Enable the ITM interface and the SWO pin.
  //  //
  //  am_hal_itm_enable();
  //
  //  //
  //  // Enable the ITM and TPIU
  //  // Set the BAUD clock for 1M
  //  //
  //  TPIUcfg.ui32SetItmBaud = AM_HAL_TPIU_BAUD_1M;
  //  am_hal_tpiu_enable(&TPIUcfg);
  //  am_hal_gpio_pincfg_t pinCfg = {
  //      .uFuncSel            = AM_HAL_PIN_41_SWO,
  //      .eDriveStrength      = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA
  //  };
  //  am_hal_gpio_pinconfig(SWD_PIN, pinCfg);
  //
  //  //
  //  // Attach the ITM to the STDIO driver.
  //  //
  //  am_util_stdio_printf_init(am_hal_itm_print);
  //
  //  //
  //  // Print the banner.
  //  //
  //  am_util_stdio_terminal_clear();
  // end of debug output

  am_hal_burst_avail_e eBurstModeAvailable;
  am_hal_burst_mode_initialize(&eBurstModeAvailable);

  am_hal_burst_mode_e eBurstMode;
  if (eBurstModeAvailable == AM_HAL_BURST_AVAIL) {
    am_hal_burst_mode_enable(&eBurstMode);  // operate at 96MHz
    am_util_stdio_printf("Enabled burst operation\n");
  } else {
    am_util_stdio_printf("Burst not availible\n");
  }

  //
  // Configure the MSPI and Flash Device.
  //
  ui32Status = am_devices_mspi_psram_init(
      MSPI_TEST_MODULE,
      (am_hal_mspi_dev_config_t*)&MSPI_Flash_Serial_CE0_MSPIConfig, &pHandle);
  if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS != ui32Status) {
    am_util_stdio_printf(
        "Failed to configure the MSPI and Flash Device correctly!\n");
    return -1;
  }

  //
  // Set up for XIP operation.
  //
  ui32Status = am_devices_mspi_psram_enable_xip();
  if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS != ui32Status) {
    am_util_stdio_printf("Failed to enable XIP mode in the MSPI!\n");
    return -2;
  }

  return 0;
}

int deinit(void) {
  uint32_t ui32Status;

  //
  // Shutdown XIP operation.
  //
  am_util_stdio_printf("Disabling the MSPI and External Flash from XIP mode\n");
  ui32Status = am_devices_mspi_psram_disable_xip();
  if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS != ui32Status) {
    am_util_stdio_printf("Failed to disable XIP mode in the MSPI!\n");
    return -2;
  }

  //
  // Clean up the MSPI before exit.
  //
  ui32Status = am_devices_mspi_psram_deinit(
      MSPI_TEST_MODULE,
      (am_hal_mspi_dev_config_t*)&MSPI_Flash_Serial_CE0_MSPIConfig);
  if (AM_DEVICES_MSPI_PSRAM_STATUS_SUCCESS != ui32Status) {
    am_util_stdio_printf("Failed to shutdown the MSPI and Flash Device!\n");
    return -1;
  }

  return 0;
}

int main(void) {
  init();

  am_util_stdio_printf("Emulator Checkpoint Test\n\n");

#ifdef CHECKPOINT
  /* Restore overwrite (i.e. restart the game) */
  am_hal_gpio_pinconfig(BUTTON_START, g_AM_HAL_GPIO_INPUT);
  am_hal_gpio_pinconfig(BUTTON_SELECT, g_AM_HAL_GPIO_INPUT);

  if (((am_hal_gpio_input_read(BUTTON_START) == 0) &&
       (am_hal_gpio_input_read(BUTTON_SELECT) == 0)) ||
      ((am_hal_gpio_input_read(BUTTON_START) == 0) &&
       (am_hal_gpio_input_read(BUTTON_A) == 0))) {
    // Force reset
    am_util_stdio_printf("Restart execution\n\n");
    checkpoint_restore_invalidate();
  }

  if (!checkpoint_restore_available()) {
    // First boot
    // Init the .bss and .data (no-restore parts already init in startup)

    // Init the remaining .data
    startup_clear_data();

    // Zero the remaining .bss
    startup_clear_bss();

    if (checkSavedCartridge((uint8_t*)GameRomStart)) {
      emulatorSetRomSize(cartridgeGetSizeBytes());
    } else {
      emulatorSetRomSize(0);
    }
  }

  if ((am_hal_gpio_input_read(BUTTON_START) == 0) &&
      (am_hal_gpio_input_read(BUTTON_A) == 0)) {
    // Read cartridge
    am_util_stdio_printf(
        "Reading cartridge to memory, takes up to 1-10 mins! \n\n");
    cartridgeConfig();

    cartridgeReadInfo();
    cartridgeReadRom();
    emulatorSetRomSize(cartridgeGetSizeBytes());
  }

  am_util_stdio_printf("Checkpoint setup (always)\n");
  checkpoint_setup();

  am_util_stdio_printf("Attempting restore\n\n");
  checkpoint_restore();

  am_util_stdio_printf("No restore available\n\n");

  am_util_stdio_printf("One-time checkpoint setup\n");
  checkpoint_onetime_setup();

  emulatorConfigIO();
  emulatorSetup();

  // First time setup (has to be after the emulatorInit()
  setup_memtracker();

  // Add the z80 memory as a "starting state"
  checkpoint_memtracker_default();

  am_util_stdio_printf("After init checkpoint\n\n");
  checkpoint();

  checkpoint_restore_set_availible();

  am_util_stdio_printf("Start emulator\n\n");
  emulatorRun();

#else
  emulatorSetup();
  emulatorRun();
#endif
}
