/*
 * emulator.c
 *
 *  Created on: Jan 23, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */


#include "am_mcu_apollo.h"
#include "am_util.h"
#include "platform.h"

#include "emulator.h"
#include "z80_ub.h"
#include "memtracker.h"

extern z80_t z80;

void memTrackerTest(){
	  z80.memory = z80_memory;

	  uint8_t testvar = 1;
	  for(uint32_t i = 0; i  < (SUB_REGIONSIZE_BYTES * NUM_MPU_TOTAL_REGIONS); i += SUB_REGIONSIZE_BYTES){
		  z80.memory[i] = testvar++; // triggers interrupt
		  z80.memory[i] = testvar++; // should not trigger interrupt
	  }
	  z80.memory[0x0] = 4;

	  int i=0;
	  while(1){
		  i++;
	  }
}

int main(void) {
	//
	  // Set the clock frequency.
	  //
	  am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, 0);

	  //
	  // Set the default cache configuration

	   am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
	   am_hal_cachectrl_enable();
//	   am_hal_cachectrl_disable();
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
	  am_util_stdio_printf("Starting emulator..\n\n");


	  am_hal_burst_avail_e eBurstModeAvailable;
	  am_hal_burst_mode_initialize(&eBurstModeAvailable);

	  am_hal_burst_mode_e  eBurstMode;
	  if(eBurstModeAvailable == AM_HAL_BURST_AVAIL) {
	  	  am_hal_burst_mode_enable(&eBurstMode); // operate at 96MHz
	  }

	  // Test case for memtracker
#if 0
	  memTrackerTest();
#else
      emulatorSetup();
      // Emulator init needs to be done first
	  initTracking(z80.memory);
	  emulatorRun();
#endif
}
