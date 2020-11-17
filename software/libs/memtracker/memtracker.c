/*
 * memtracker.c
 *
 *  Created on: Mar 31, 2020
 *      Author: TU Delft Sustainable Systems Laboratory
 *     License: MIT License
 */

#include "memtracker.h"

#include <string.h>

#include "am_mcu_apollo.h"
#include "platform.h"

CHECKPOINT_EXCLUDE_BSS
uint32_t regionTracker[NUM_MPU_TOTAL_REGIONS];

uint32_t startAddress;

#ifdef TRACKING_COUNT_WRITES
typedef void (*CompleteWriteFunctionPtr)(uint32_t stackptr);

// buffer is aligned and oversized to make sure it does not overflow into data.
CHECKPOINT_EXCLUDE_BSS
uint32_t __attribute__((aligned(32))) completeWriteFunctionBuffer[200];

CHECKPOINT_EXCLUDE_BSS
CompleteWriteFunctionPtr completeWriteFunctionMem;

// This function does the write that triggered the interrupt handler.
// To do the skipped write:
// - First to be safe stack all registers
//   the following registers are stacked:
//   xPSR PC LR r12 r3 r2 r1 r0.
// - Restore r12 r3 r2 r1 r0 from the stack.
// - The data to write should already be in the memory since
//   the mem isr is triggered by the write.
// - Copy the write instruction into memory.
// - Execute the instruction from memory.
// - Restore and jump back.
void __attribute__((naked)) CompleteWriteASM(uint32_t stackptr) {
  __asm(" push    {r0,r1,r2,r3,r12}");  // Push everything to be safe
  __asm(" ldr     r12, [r0, #16]");     // Restore r12
  __asm(" ldr     r3, [r0, #12]");      // Restore r3

  // Increment the pc to skip the write
  __asm(" ldr     r2, [r0, #24]");  // Get stacked program counter
  __asm(" add     r2, r2, #2");     // Increment the stacked PC
  __asm(" str     r2, [r0, #24]");  // Store the stacked PC to skip the write
  __asm(" sub     r2, r2, #2");     // Remote the increment to the stacked PC

  // do the skipped write
  __asm(" ldrh    r2, [r2]");  // Get the thumb 16 bit instruction from the
                               // stacked PC
  __asm(" mov     r1, pc");    // Copy the current program counter.
  __asm(" add     r1, #12");  // Add 12 to the program counter, making space for
                              // restoring r0,r1 and r2.
  __asm(" strh    r2, [r1]");      // Write the thumb write instruction into the
                                   // execution path
  __asm(" ldr     r2, [r0, #8]");  // Restore r2
  __asm(" ldr     r1, [r0, #4]");  // Restore r1
  __asm(" ldr     r0, [r0, #0]");  // Restore r0
  __asm(" nop     ");              // Some padding just to be safe
  __asm(" nop     ");              // Instruction to be repace by the write
  __asm(" nop     ");              // Some padding just to be safe

  __asm(" pop     {r0,r1,r2,r3,r12}");  // Everything from stack
  __asm(" bx      lr");  // Return to a wonderful world where the write happened
}
#endif

void initTracking(uint8_t* z80RamPtr) {
  uint32_t regionAddress = ((uint32_t)z80RamPtr) & ~REGIONMASK;
  startAddress = regionAddress;

  /* Disable MPU */
  ARM_MPU_Disable();

  for (uint8_t i = 0; i < NUM_MPU_REGIONS; i++) {
    // register the region to the fraction of the memory size
    MPU->RBAR = ARM_MPU_RBAR(i, regionAddress);
    // enable all subregions and set to read only, i.e. writes trigger
    // MemManage_Handler.
    MPU->RASR = ARM_MPU_RASR(0, ARM_MPU_AP_RO, 0, 0, 1, 1, 0x00, REGIONSIZE);
    regionAddress += (2 << REGIONSIZE);
  }

  // clear te buffer to make sure.
  for (uint16_t i = 0; i < NUM_MPU_TOTAL_REGIONS; ++i) {
    regionTracker[i] = 0;
  }

  /* Enable MPU */
  ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);

#ifdef TRACKING_COUNT_WRITES
  memcpy(completeWriteFunctionBuffer,
         (uint32_t*)((uint32_t)&CompleteWriteASM & 0xFFFFFFFE), 100);
  completeWriteFunctionMem =
      (CompleteWriteFunctionPtr)((uint32_t)completeWriteFunctionBuffer | 0x01);
#endif
}

void memTrackingHandler(uint32_t stackptr) {
  // get the address causing the issue MIGHT BE INVALID!
  uint32_t address = SCB->MMFAR;
  address -= startAddress;
  // Check if MPU caused the interrupt. (MMFAR is shared with BFAR!)
  if (SCB->CFSR & SCB_CFSR_MMARVALID_Msk) {
    // Reset CFSR otherwise MMFAR remains the same.
    SCB->CFSR |= SCB->CFSR;

    // Apply mask to get the subregion that triggered the handler.
    address &= ~SUBREGIONMASK;
    // Retrieve the region.
    uint8_t subregion = address / SUB_REGIONSIZE_BYTES;

    // Count the write
    regionTracker[subregion]++;

    // Select the correct region
    MPU->RNR = subregion / 8;
    // Disable the region.
    MPU->RASR &= ~MPU_RASR_ENABLE_Msk;

    // Redo the write.
#ifdef TRACKING_COUNT_WRITES
    completeWriteFunctionMem(stackptr);
#endif

#ifdef TRACKING_COUNT_WRITES
    // keep the subregion enabled to track the number of writes
    MPU->RASR |= MPU_RASR_ENABLE_Msk;
#else
    // Disable subregion triggering the handler,
    // no need for multiple calls to the handler since it needs
    // to be checkpointed anyway.
    MPU->RASR |= MPU_RASR_ENABLE_Msk | (1UL << ((subregion % 8) + 8));
#endif
  }
}

void __attribute__((naked)) MemManage_Handler(void) {
  __asm(" push    {r0,lr}");
  __asm(" tst     lr, #4");
  __asm(" itet    eq");
  __asm(" mrseq   r0, msp");
  __asm(" mrsne   r0, psp");
  __asm(" addseq  r0, r0, #8");
  __asm(" bl      memTrackingHandler");
  __asm(" pop     {r0,pc}");
}
