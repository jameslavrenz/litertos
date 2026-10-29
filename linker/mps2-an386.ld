/*
 * Phase 1 — QEMU mps2-an386 only. Phase 2 uses a separate STM32F446 script.
 * Steps 1–5 of Phase 1 all link with this file. Step 1's host test does not.
 *
 * QEMU mps2-an386 memory map (ARM AN386, as implemented by QEMU):
 *
 *   0x00000000  4 MiB  code RAM (ZBT SSRAM1)
 *   0x20000000  4 MiB  data RAM (ZBT SSRAM2 and SSRAM3)
 *
 * The initial MSP is the top of data RAM. Task stacks are ordinary .bss
 * objects lower in that RAM. UART0, used by the demo, is at 0x40004000
 * and is not described here because it is a peripheral, not a memory region
 * the linker places sections into.
 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 4M
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 4M
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector : {
        KEEP(*(.isr_vector))
    } > FLASH

    .text : {
        *(.text*)
        *(.rodata*)
        _etext = .;
    } > FLASH

    .data : {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > RAM AT > FLASH

    _sidata = LOADADDR(.data);

    .bss : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > RAM

    /DISCARD/ : {
        *(.ARM.exidx*)
        *(.ARM.extab*)
        *(.note.gnu.property)
        *(.comment)
    }
}
