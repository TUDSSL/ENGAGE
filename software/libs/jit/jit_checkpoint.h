/*
 * jit_checkpoint.h
 *
 *  Created on: Jan 23, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#ifndef JIT_CHECKPOINT_H_
#define JIT_CHECKPOINT_H_

#include "emulator_settings.h"
#include "platform.h"

#define jitTriggerThreshold ((jitTriggermV * 1024) / 557) << 6

#define JIT_ADC_DEBUG 0
#define ADC_SAMPLE_RATE 32

#define JIT_ADC_PIN ADC_VBAT_JIT
#define JIT_ADC_PIN_CFG AM_HAL_PIN_29_ADCSE1

#define JIT_TIMER_CLOCK AM_HAL_CTIMER_LFRC_32HZ
#define JIT_TIMER_PERIOD ADC_SAMPLE_RATE

#define JIT_COMPARE 42000

// The JIT flag, becomes 1 when we need to checkpoint
extern volatile char jit_checkpoint;
extern volatile uint16_t adcMeasurement;

void jit_setup(void);

#endif /* JIT_CHECKPOINT_H_ */
