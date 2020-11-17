.syntax unified
.cpu cortex-m4
.fpu softvfp

.thumb

/*
    NB: The Atmel samd21 used has no privileged mode so only uses MSP never PSP
*/

.global SVC_Handler
.type SVC_Handler, %function
SVC_Handler:
    /*
    +===================+============================================+
    | *Exception Frame*  after entering ISR                          |
    +===================+============================================+
    | Process stack     | <- SP before interrupt                     |
    +-------------------+--------------------------------------------+
    | xPSR              |                                            |
    +-------------------+--------------------------------------------+
    | PC                |                                            |
    +-------------------+--------------------------------------------+
    | LR                |                                            |
    +-------------------+--------------------------------------------+
    | r12               |                                            |
    +-------------------+--------------------------------------------+
    | r3                |                                            |
    +-------------------+--------------------------------------------+
    | r2                |                                            |
    +-------------------+--------------------------------------------+
    | r1                |                                            |
    +-------------------+--------------------------------------------+
    | r0                | <- SP after entering interrupt SVC_Handler |
    +-------------------+--------------------------------------------+

    register checkpoint array layout
    +---------------------+--------------+
    | registers[17] index |   Register   |
    +---------------------+--------------+
    |                   0 | SP ISR start |
    |                   1 | r12          |
    |                   2 | LR           |
    |                   3 | PC           |
    |                   4 | xPSR         |
    |                   5 | r0           |
    |                   6 | r1           |
    |                   7 | r2           |
    |                   8 | r3           |
    |                   9 | r8           |
    |                  10 | r9           |
    |                  11 | r10          |
    |                  12 | r11          |
    |                  13 | r4           |
    |                  14 | r5           |
    |                  15 | r6           |
    |                  16 | r7           |
    +---------------------+--------------+

    Pseudocode flow:
        - disable interrupts
        - if checkpoint_scv_restore == 1: goto b_restore
        Checkpoint:
            - push r4-r7 to registers[13-16]
            - push r8-r11 to registers[9-12]
            - copy r0-r3 from exception stack to registers[5-8]
            - copy r12, LR, PC, xPSR from exception stack to registers[1-4]
            - copy SP after entering interrupt to registers[0]
            - goto b_cleanup

        Restore (b_restore):
            - load r0 with &registers[0]
            - load r1 with registers[1] (SP after entering interrupt)
            - copy r12, LR, PC, xPSR from registers[1-4] to exception stack
            - copy r0-r3 from registers[5-8] to exception stack
            - load r8-r11 from registers[8-12]
            - load r4-r7 from registers[13-16]
            - goto b_cleanup

        Cleanup (b_cleanup):
            - indicate return type
            - enable interrupts
            - branch return
    */

    /* Disable interrupts: */
    cpsid   i

    /* If global variable pendsv_restore > 0 jump to restore */
    ldr     r0, =checkpoint_svc_restore
    ldr     r0, [r0]
    cmp     r0, #0
    bgt     b_restore

    /*
    Checkpoint
    */
    ldr     r0, =registers_top
    ldr     r0, [r0]
    subs    r0, #16
    stmia   r0!,{r4-r7}
    mov     r4, r8
    mov     r5, r9
    mov     r6, r10
    mov     r7, r11
    subs    r0, #32
    stmia   r0!,{r4-r7}
    subs    r0, #16

    /* Copy registers saved to msp to the registers array */
    mrs     r1, msp

    ldmia   r1!,{r4-r7} // r0, r1, r2, r3
    subs    r0, #16
    stmia   r0!,{r4-r7} // push r0, r1, r2, r3 to the register array

    ldmia   r1!,{r4-r7} // r12, LR, PC, xPSR
    subs    r0, #32
    stmia   r0!,{r4-r7} // push r12, LR, PC, xPSR to the register array

    subs    r0, #16

    /* save the sp */
    mrs     r1, msp
    subs    r0, #4
    str     r1, [r0]

    /* restore the used registers (r4-r7) */
    ldr     r0, =registers_top
    ldr     r0, [r0]
    subs    r0, #16
    ldmia   r0!,{r4-r7}

    b       b_cleanup

    /*
    Restore
    */
    b_restore:

    // Restore the SP from the checkpoint
    ldr     r0, =registers // sp is registers[0]
    ldr     r1, [r0]

    // r0 = registers
    // r1 = saved sp after entering interrupt

    adds    r0, #4 // point to registers[1] (end of checkpoint)

    ldmia   r0!,{r4-r7} // load r12, LR, PC, xPSR
    adds    r1, #16
    stmia   r1!,{r4-r7} // push r12, LR, PC, xPSR to the stack before the ISR

    ldmia   r0!,{r4-r7} // load r0, r1, r2, r3
    subs    r1, #32
    stmia   r1!,{r4-r7} // push r0, r1, r2, r3 to the stack before the ISR

    subs    r1, #16
    msr     msp, r1

    ldmia   r0!,{r4-r7} // load r8, r9, r10, r11
    mov     r8, r4
    mov     r9, r5
    mov     r10, r6
    mov     r11, r7

    ldmia   r0!,{r4-r7} // load r4, r5, r6, r7

    /*
    Cleanup
    */
    b_cleanup:

    /* EXC_RETURN - Return to Handler mode with MSP */
    ldr     r0, =0xFFFFFFE9

    /* Enable interrupts: */
    cpsie   i
    dsb
    isb
    bx      r0

.size SVC_Handler, .-SVC_Handler

