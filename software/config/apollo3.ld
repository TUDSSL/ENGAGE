ENTRY(Reset_Handler)

MEMORY
{
    ROMEM (rx) : ORIGIN = 0x0000C000, LENGTH = 448K
    GBMEM (rx) : ORIGIN = 0x00080000, LENGTH = 512K
    RWMEM (rwx) : ORIGIN = 0x10000000, LENGTH = 384K
    NVMEM (rwx) : ORIGIN = 0x51000000, LENGTH = 512K
}

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        KEEP(*(.patch))
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > ROMEM
    
    .gb (NOLOAD) :
    {
        . = ALIGN(4);
        _s_data_gb = .;
        . = ORIGIN(GBMEM) + LENGTH(GBMEM) - 1;
        . = ALIGN(4);
        _e_data_gb = .;
    } > GBMEM

    /* User stack section initialized by startup code. */
    .stack (NOLOAD):
    {
        . = ALIGN(8);
        *(.stack)
        *(.stack*)
        . = ALIGN(8);
        _estack = .;
    } > RWMEM

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        _sdata_norestore = .;
        *(.norestore_data)
        *(.norestore_data*)
        . = ALIGN(4);
        INCLUDE "exclude_checkpoint_data.ld" ;
        . = ALIGN(4);
        _edata_norestore = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RWMEM AT>ROMEM

    /* used by startup to initialize data */
    _init_data = LOADADDR(.data);

    /* Need to add NOLOAD to keep the type from changing to PROGBITS */
    .bss (NOLOAD):
    {
        . = ALIGN(4);
        _sbss = .;
        _sbss_norestore = .;
        *(.norestore_bss)
        *(.norestore_bss*)
        . = ALIGN(4);
        INCLUDE "exclude_checkpoint_bss.ld" ;
        . = ALIGN(4);
        _ebss_norestore = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RWMEM

    .restore_stack (NOLOAD):
    {
        . = ALIGN(4);
        . = . + 1K; /* Reserve 1K for the restore stack (TODO check actual requirement) */
        . = ALIGN(4);
        _erestore_stack = .;
    } > RWMEM

    .nvm (NOLOAD):
    {
        . = ALIGN(4);

        _data_checkpoint_0_start = .;
        . = ALIGN(4);
        . = DEFINED(_checkpoint_data_allocate_checkpoint_ld) ? . + (_edata - _edata_norestore) : . ;
        . = ALIGN(4);
        _data_checkpoint_1_start = .;
        . = . + _data_checkpoint_1_start - _data_checkpoint_0_start;
        _data_checkpoint_1_end = .;

        /* .bss checkpoint */
        _bss_checkpoint_0_start = .;
        . = ALIGN(4);
        . = DEFINED(_checkpoint_data_allocate_checkpoint_ld) ? . + (_ebss - _ebss_norestore) : . ;
        . = ALIGN(4);
        _bss_checkpoint_1_start = .;
        . = . + _bss_checkpoint_1_start - _bss_checkpoint_0_start;
        _bss_checkpoint_1_end = .;

        . = ALIGN(4);
        _snvm = .;
        *(.nvm)
        *(.nvm*)
        . = ALIGN(4);
        _envm = .;

        _sbliss = .;
        . = ORIGIN(NVMEM) + LENGTH(NVMEM) - 1;
        . = ALIGN(4);
        _ebliss = .;
    } > NVMEM

    /DISCARD/ :
    {
        *(.allocate_checkpoint_flags)
        *(.allocate_checkpoint_flags*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
