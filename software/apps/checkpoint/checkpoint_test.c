//*****************************************************************************
//
// Copyright (c) 2019, Ambiq Micro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision 2.3.2 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include "am_mcu_apollo.h"
#include "am_util.h"
#include "fram.h"
#include "platform.h"
#include "mspi.h"

#include "checkpoint.h"
#include "mpatch.h"

#define MSPI_TEST_MODULE 0

#if defined(AM_PART_APOLLO3)
#define MSPI_XIP_BASE_ADDRESS 0x04000000
#define MSPI_XIPMM_BASE_ADDRESS 0x51000000
#define MSPI_XIPMM_SIZE 0x100000
#elif defined(AM_PART_APOLLO3P)
#define MSPI_XIP_BASE_ADDRESS 0x02000000
#endif

//#define START_SPEED_INDEX       0
//#define END_SPEED_INDEX         11

CHECKPOINT_EXCLUDE_BSS uint32_t DMATCBBuffer[2560];

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
    .ui32TCBSize = (sizeof(DMATCBBuffer) / sizeof(uint32_t)),
    .pTCB = DMATCBBuffer,
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
  // am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
//   am_hal_cachectrl_enable();
   am_hal_cachectrl_disable();
  //
  // Initialize for low power in the power control block
  //
  am_hal_pwrctrl_low_power_init();

  //
  // Disable the RTC.
  //
  am_hal_rtc_osc_disable();

  am_hal_tpiu_config_t TPIUcfg;
  //
  // Set the global print interface.
  //

  //
  // Enable the ITM interface and the SWO pin.
  //
  am_hal_itm_enable();

  //
  // Enable the ITM and TPIU
  // Set the BAUD clock for 1M
  //
  TPIUcfg.ui32SetItmBaud = AM_HAL_TPIU_BAUD_1M;
  am_hal_tpiu_enable(&TPIUcfg);
  am_hal_gpio_pincfg_t pinCfg = {
      .uFuncSel            = AM_HAL_PIN_41_SWO,
      .eDriveStrength      = AM_HAL_GPIO_PIN_DRIVESTRENGTH_2MA
  };
  am_hal_gpio_pinconfig(SWD_PIN, pinCfg);

  //
  // Attach the ITM to the STDIO driver.
  //
  am_util_stdio_printf_init(am_hal_itm_print);

  //
  // Print the banner.
  //
  am_util_stdio_terminal_clear();


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

//*****************************************************************************
//
// MSPI Example Main.
//
//*****************************************************************************

// Debugging
#include "bliss_allocator.h"
#include "bliss_allocator_util.h"
extern bliss_block_idx_t bliss_number_of_blocks;

CHECKPOINT_EXCLUDE_DATA uint32_t test_patch_restore = 10;
CHECKPOINT_EXCLUDE_DATA uint32_t test_patch_norestore = 10;


/*
 * Test variables
 */
int test_global = 10; // .data
static int test_static_global; // .bss and static
CHECKPOINT_EXCLUDE_DATA int test_global_norestore = 10; // .data without restore


int main(void) {
  init();

  am_util_stdio_printf("Apollo3 Checkpoint Test\n\n");

  if (!checkpoint_restore_available()) {
    // First boot
    // Init the .bss and .data (no-restore parts already init in startup)

    // Init the remaining .data
    startup_clear_data();

    // Zero the remaining .bss
    startup_clear_bss();
  }

  int test_local = 10;

  am_util_stdio_printf("Checkpoint setup (always)\n");
  checkpoint_setup();

  am_util_stdio_printf("Attempting restore\n\n");
  checkpoint_restore();
  am_util_stdio_printf("No restore available\n\n");

  am_util_stdio_printf("One-time checkpoint setup\n");
  checkpoint_onetime_setup();

  am_util_stdio_printf("After init checkpoint\n\n");
  checkpoint();

  checkpoint_restore_set_availible();


  while (1) {
    test_global += 1;
    test_static_global += 1;
    test_local += 1;
    test_global_norestore += 1;

    //am_util_stdio_printf("Number of free blocks: %d\n", (int)bliss_active_allocator->n_free_blocks);
    test_patch_restore += 1;
    test_patch_norestore += 1;
    mpatch_pending_patch_t pp;
    mpatch_new_region(&pp,
                      (mpatch_addr_t)&test_patch_restore,
                      (mpatch_addr_t)&mpatch_util_last_byte(test_patch_restore),
                      MPATCH_STANDALONE);
    mpatch_stage_patch(MPATCH_GENERAL, &pp);

    am_util_stdio_printf("Test global variable (.data): %d\n", test_global);
    am_util_stdio_printf("Test global variable (.bss): %d\n", test_static_global);
    am_util_stdio_printf("Test local variable (.stack): %d\n", test_local);
    am_util_stdio_printf("Test global variable (.norestore_data): %d\n", test_global_norestore);
    am_util_stdio_printf("Test mpatch variable: %d\n", test_patch_restore);
    am_util_stdio_printf("Test mpatch no-rest variable: %d\n", test_patch_norestore);
    am_util_stdio_printf("\n");

    am_util_delay_ms(500);
    checkpoint();
  }

  deinit();
}
