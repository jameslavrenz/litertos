/*
 * Phase 1 — QEMU mps2-an386 only. Not used in Phase 2.
 * Step 2 — Stop the demo after the yield transcript. Not a kernel service.
 * Steps 3, 4, and 5 keep this exit for their QEMU runs.
 *
 * BKPT #0xAB with r0 = 0x18 (SYS_EXIT) and r1 = 0x20026
 * (ADP_Stopped_ApplicationExit) is the AArch32 semihosting exit.
 * QEMU treats it as a guest exit when started with -semihosting.
 * Without that flag the same BKPT becomes a fault.
 */
.syntax unified
.cpu cortex-m4
.thumb

.section .text, "ax", %progbits

.global qemu_exit_ok
.thumb_func
qemu_exit_ok:
    movs r0, #0x18
    movw r1, #0x0026
    movt r1, #0x0002
    bkpt 0xAB
    b .
