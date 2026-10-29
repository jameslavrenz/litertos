/*
 * Phase 1 — QEMU mps2-an386. Shared with Phase 2 (STM32F446): yes.
 * Step 2 — Cooperative multitasking. The save/restore sequence moves to
 * PendSV in Step 4; the register order must not change.
 * Step 3 does not edit this file. It changes which TCB litert_switch_next
 * returns. Step 5 does not edit this file either.
 *
 * Cooperative context switch through SVC.
 *
 * A task calls litert_yield(), which executes SVC #0. Exception entry is
 * what makes the stack match the frame built by litert_task_init():
 *
 *   hardware pushes R0, R1, R2, R3, R12, LR, PC, xPSR onto the task's PSP
 *   this handler pushes R4-R11 with STMDB in front of that frame
 *
 * The handler itself runs on MSP. BX LR with EXC_RETURN in LR tells the
 * processor to pop the hardware frame from PSP and resume Thread mode.
 * EXC_RETURN is not a code address. It is only meaningful as the value
 * branched to while in Handler mode, which is why the first task is
 * started by an SVC rather than by a branch from main.
 *
 *   0xFFFFFFFD
 *     bit 3 = 1  return to Thread mode
 *     bit 2 = 1  unstack from the PSP, and set CONTROL.SPSEL
 *     bit 4 = 1  integer frame, no floating-point context
 *
 * PendSV and SysTick are Step 4. Their vector entries still branch to
 * Default_Handler. Phase 2 uses this same handler.
 */
.syntax unified
.cpu cortex-m4
.thumb

.equ EXC_RETURN_THREAD_PSP, 0xFFFFFFFD

.section .text, "ax", %progbits

.global litert_yield
.thumb_func
litert_yield:
    svc #0
    bx lr

.global SVC_Handler
.thumb_func
SVC_Handler:
    ldr r0, =litert_running
    ldr r1, [r0]
    cbnz r1, .Lsave

    /* First SVC, still coming from main on MSP. Do not save main.
     * Drop the frame this SVC pushed and put MSP back at the top of
     * the kernel stack. The task stack is restored below. */
    movs r1, #1
    str r1, [r0]
    ldr r0, =_estack
    msr msp, r0
    ldr r1, =litert_current
    ldr r0, [r1]
    b .Lrestore

.Lsave:
    /* PSP points at the hardware frame. STMDB stores R4 at the new PSP
     * and R11 just below R0, which is the layout litert_task_init() used. */
    mrs r0, psp
    stmdb r0!, {r4-r11}
    ldr r1, =litert_current
    ldr r2, [r1]
    str r0, [r2]                 /* current->sp = updated PSP */
    bl litert_switch_next        /* r0 = the other TCB; LR is clobbered */

.Lrestore:
    ldr r0, [r0]                 /* first field of the TCB is sp */
    ldmia r0!, {r4-r11}
    msr psp, r0                  /* PSP now points at the hardware frame */
    ldr lr, =EXC_RETURN_THREAD_PSP
    bx lr

.ltorg
