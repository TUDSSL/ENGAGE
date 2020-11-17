/*
 * jit_checkpoint.c
 *
 *  Created on: Jan 23, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#include "jit_checkpoint.h"

#include "am_mcu_apollo.h"
#include "am_util.h"
#ifdef CHECKPOINT
#include "checkpoint.h"
#endif

static void adc_config(void);
static void adc_deconfig(void);

// ADC Device Handle.
#ifdef CHECKPOINT
CHECKPOINT_EXCLUDE_BSS
static void* g_ADCHandle;
#else
static void* g_ADCHandle;
#endif

#ifdef CHECKPOINT
CHECKPOINT_EXCLUDE_DATA
volatile char jit_checkpoint = 0;
#else
volatile char jit_checkpoint = 0;
#endif
CHECKPOINT_EXCLUDE_DATA
volatile uint16_t adcMeasurement = 0;

// ADC Pin
const am_hal_gpio_pincfg_t g_AM_PIN_29_ADCSE1 = {
    .uFuncSel = JIT_ADC_PIN_CFG,
};

void am_adc_isr(void) {
  uint32_t ui32IntMask;
  am_hal_adc_sample_t Sample;

  // Read the interrupt status.
  am_hal_adc_interrupt_status(g_ADCHandle, &ui32IntMask, false);

  // Clear the ADC interrupt.
  am_hal_adc_interrupt_clear(g_ADCHandle, ui32IntMask);

  if (ui32IntMask & AM_HAL_ADC_INT_WCEXC) {
    // Outside of window, trigger jit
    jit_checkpoint = 1;
#if JIT_ADC_DEBUG
    am_util_stdio_printf("[jit] Triggered\n");
#endif
  }
  if (ui32IntMask & AM_HAL_ADC_INT_CNVCMP) {
    uint32_t ui32NumSamples = 1;
    am_hal_adc_samples_read(g_ADCHandle, false, NULL, &ui32NumSamples, &Sample);
    adcMeasurement = Sample.ui32Sample;
  }
}

static void adc_config(void) {
  // Set up the ADC configuration parameters
  am_hal_adc_config_t ADCConfig = {
      .eClock = AM_HAL_ADC_CLKSEL_HFRC_DIV2,
      .eTrigger = AM_HAL_ADC_TRIGSEL_SOFTWARE,
      .eReference = AM_HAL_ADC_REFSEL_INT_2P0,
      .eClockMode = AM_HAL_ADC_CLKMODE_LOW_POWER,
      .ePowerMode = AM_HAL_ADC_LPMODE1,
      .eRepeat = AM_HAL_ADC_REPEATING_SCAN,
  };

  am_hal_adc_slot_config_t ADCSlotConfig = {
      .eMeasToAvg = AM_HAL_ADC_SLOT_AVG_1,
      .ePrecisionMode = AM_HAL_ADC_SLOT_14BIT,
      .eChannel = AM_HAL_ADC_SLOT_CHSEL_SE1,
      .bWindowCompare = true,
      .bEnabled = true,
  };

  am_hal_adc_window_config_t ADCWindowConfig = {
      .bScaleLimits = false,
      .ui32Upper = 0xFFFFF,
      .ui32Lower = jitTriggerThreshold};

  // Initialize the ADC and get the handle.
  am_hal_adc_initialize(0, &g_ADCHandle);

  // Power on the ADC.
  am_hal_adc_power_control(g_ADCHandle, AM_HAL_SYSCTRL_WAKE, false);

  am_hal_adc_configure(g_ADCHandle, &ADCConfig);

  // Set up comparator
  am_hal_adc_control(g_ADCHandle, AM_HAL_ADC_REQ_WINDOW_CONFIG,
                     &ADCWindowConfig);

  // Set up an ADC slot
  am_hal_adc_configure_slot(g_ADCHandle, 0, &ADCSlotConfig);

  am_hal_adc_interrupt_enable(g_ADCHandle,
                              AM_HAL_ADC_INT_CNVCMP | AM_HAL_ADC_INT_WCEXC);

  //
  // Enable the ADC.
  //
  am_hal_adc_enable(g_ADCHandle);
}

static void adc_deconfig(void) {
  //
  // Disable the ADC.
  //
  if (AM_HAL_STATUS_SUCCESS != am_hal_adc_disable(g_ADCHandle)) {
    am_util_stdio_printf("Error - disable ADC failed.\n");
  }

  //
  // Enable the ADC power domain.
  //
  if (AM_HAL_STATUS_SUCCESS !=
      am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_ADC)) {
    am_util_stdio_printf("Error - disabling the ADC power domain failed.\n");
  }

  //
  // Initialize the ADC and get the handle.
  //
  if (AM_HAL_STATUS_SUCCESS != am_hal_adc_deinitialize(g_ADCHandle)) {
    am_util_stdio_printf("Error - return of the ADC instance failed.\n");
  }
}

void triggerAdc(void) { am_hal_adc_sw_trigger(g_ADCHandle); }

static void enableAdcInterrupts(void) {
  NVIC_EnableIRQ(ADC_IRQn);
  am_hal_interrupt_master_enable();
}

#define AdcTimer 3
#define ComTimerClkSource AM_HAL_CTIMER_HFRC_12KHZ
#define PERIOD 10
#define NUM_PULSES_PERIOD ((12000 / (PERIOD * 2)) - 1)

void jitTimerInit() {
  //
  // Configure a timer to drive the LED.
  //
  am_hal_ctimer_config_single(
      AdcTimer, AM_HAL_CTIMER_TIMERA,
      (AM_HAL_CTIMER_FN_REPEAT | ComTimerClkSource | AM_HAL_CTIMER_ADC_TRIG));

  //
  // Set up initial timer periods.
  //
  am_hal_ctimer_period_set(AdcTimer, AM_HAL_CTIMER_TIMERA, NUM_PULSES_PERIOD,
                           0);

  //
  // Start the timer.
  //
  am_hal_ctimer_start(AdcTimer, AM_HAL_CTIMER_TIMERA);
}

void jit_setup(void) {
  am_hal_gpio_pinconfig(JIT_ADC_PIN, g_AM_PIN_29_ADCSE1);

  jitTimerInit();
  adc_config();
  enableAdcInterrupts();
  jit_checkpoint = 0;
  triggerAdc();  // first trigger needs to be in software next is all done in hw
}
