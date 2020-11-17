#include "checkpoint_setup.h"

#include "am_mcu_apollo.h"

void checkpoint_setup(void)
{
    NVIC_SetPriority(SVCall_IRQn, 0xff); /* Lowest possible priority */
}
