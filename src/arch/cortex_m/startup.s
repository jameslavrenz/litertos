/*
 * Phase 1 — QEMU mps2-an386. Phase 2 (STM32F446) replaces this file.
 * Step 2 — Reset, vector table, and the call to main. The kernel switch
 * is in context.s. Step 1 has no startup code.
 *
 * Step 4 fills in SysTick_Handler and PendSV_Handler. Both are
 * Default_Handler until then. Step 5 replaces HardFault_Handler; the
 * Step 2 demo in demo_main.c overrides it for now.
 * Step 3 does not edit this file.
 *
 * Reset vector table and startup for QEMU mps2-an386.
 *
 * The table must be the first thing in the image. Word 0 is the initial
 * Main Stack Pointer. Word 1 is Reset_Handler. The CPU loads both before
 * any C code runs. Handler addresses are Thumb symbols, so the linker
 * sets bit 0. The CPU clears that bit and uses it as the EPSR.T state.
 */
.syntax unified
.cpu cortex-m4
.thumb

.section .isr_vector, "a", %progbits
.global vector_table
vector_table:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler          /* Phase 1 Step 2: start and yield */
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler       /* Phase 1 Step 4: switch. Step 2: Default_Handler */
    .word SysTick_Handler      /* Phase 1 Step 4: tick.   Step 2: Default_Handler */

.section .text, "ax", %progbits

.thumb_func
Default_Handler:
    b Default_Handler

.weak NMI_Handler
.thumb_set NMI_Handler, Default_Handler
.weak HardFault_Handler
.thumb_set HardFault_Handler, Default_Handler
.weak MemManage_Handler
.thumb_set MemManage_Handler, Default_Handler
.weak BusFault_Handler
.thumb_set BusFault_Handler, Default_Handler
.weak UsageFault_Handler
.thumb_set UsageFault_Handler, Default_Handler
.weak DebugMon_Handler
.thumb_set DebugMon_Handler, Default_Handler
.weak PendSV_Handler
.thumb_set PendSV_Handler, Default_Handler
.weak SysTick_Handler
.thumb_set SysTick_Handler, Default_Handler

/*
 * Copy .data from its load address in the code image into RAM, then zero
 * .bss. main() is a normal C call and uses MSP. Tasks are not running yet.
 */
.global Reset_Handler
.thumb_func
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
.copy_data:
    cmp r1, r2
    bhs .zero_bss
    ldr r3, [r0], #4
    str r3, [r1], #4
    b .copy_data
.zero_bss:
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
.zero_loop:
    cmp r1, r2
    bhs .call_main
    str r3, [r1], #4
    b .zero_loop
.call_main:
    bl main
    b .
