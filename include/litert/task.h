/*
 * LiteRTOS — task creation and initial stack frame.
 *
 * This header is the Cortex-M (no FPU) contract for a brand-new task.
 * Nothing here switches tasks. A later increment's context restore must
 * pop registers in the order documented below; portable scheduler code
 * should treat the frame as private to the architecture port.
 *
 * Pointer width
 * -------------
 * Frame slots are uint32_t because a Cortex-M register is 32 bits.
 * The TCB's sp member is a C pointer to those slots, so it is 32 bits
 * when this file is compiled for Cortex-M and 64 bits when the same
 * sources are unit-tested on a 64-bit host. The values written into the
 * frame are always 32-bit images of registers, never host pointers.
 */
#ifndef LITERT_TASK_H
#define LITERT_TASK_H

#include <stdint.h>

/*
 * Initial exception frame, lowest address first.
 *
 * A future context switch saves callee-saved registers with the ARM
 * instruction STMDB sp!, {r4-r11}, which stores R4 at the lowest address
 * and R11 just below the hardware frame. Exception entry hardware then
 * stacks the remaining registers in the fixed order R0, R1, R2, R3, R12,
 * LR, PC, xPSR (ARMv7-M). Building both parts now means the first restore
 * can be the inverse of that sequence.
 *
 *   higher addresses
 *   +------------------+
 *   | xPSR             |  [15]  bit 24 (T) set: return into Thumb state
 *   | PC               |  [14]  task entry, bit 0 cleared
 *   | LR               |  [13]  where the task goes if its function returns
 *   | R12              |  [12]
 *   | R3               |  [11]
 *   | R2               |  [10]
 *   | R1               |  [9]
 *   | R0               |  [8]   task argument (AAPCS first argument)
 *   +------------------+  hardware frame (pushed by the processor)
 *   | R11              |  [7]
 *   | R10              |  [6]
 *   | R9               |  [5]
 *   | R8               |  [4]
 *   | R7               |  [3]
 *   | R6               |  [2]
 *   | R5               |  [1]
 *   | R4               |  [0]   <-- tcb->sp, the future PSP, points here
 *   +------------------+  software frame (pushed by STMDB {r4-r11})
 *   lower addresses
 *
 * EXC_RETURN is not stored in this frame. It will live in the LR register
 * of the exception handler that eventually performs the first restore.
 *
 * The task runs on the process stack pointer (PSP). The main stack pointer
 * (MSP) stays reserved for exceptions and kernel entry. This function does
 * not write CONTROL or switch stack pointers; it only prepares memory.
 *
 * No floating-point context is stacked. The port assumes the software
 * floating-point ABI and a clear CONTROL.FPCA, so the extended 26-word
 * frame (S0-S15, FPSCR, plus the integer frame) is not used.
 */
enum {
    LITERT_FRAME_R4 = 0,
    LITERT_FRAME_R5,
    LITERT_FRAME_R6,
    LITERT_FRAME_R7,
    LITERT_FRAME_R8,
    LITERT_FRAME_R9,
    LITERT_FRAME_R10,
    LITERT_FRAME_R11,
    LITERT_FRAME_R0,
    LITERT_FRAME_R1,
    LITERT_FRAME_R2,
    LITERT_FRAME_R3,
    LITERT_FRAME_R12,
    LITERT_FRAME_LR,
    LITERT_FRAME_PC,
    LITERT_FRAME_XPSR,
    LITERT_FRAME_WORDS
};

/* EPSR.T (bit 24). Cortex-M cannot execute ARM state; T == 0 is a UsageFault. */
#define LITERT_INITIAL_XPSR 0x01000000u

/* Stacked PC bit 0 is not a Thumb flag. The T bit above carries that. */
#define LITERT_PC_THUMB_MASK 0xFFFFFFFEu

typedef enum litert_status {
    LITERT_OK = 0,
    LITERT_ERR_PARAM,    /* NULL tcb or NULL stack */
    LITERT_ERR_NO_STACK, /* stack_words < LITERT_FRAME_WORDS */
    LITERT_ERR_ALIGN     /* buffer or top of stack is not 8-byte aligned */
} litert_status_t;

/*
 * sp is first so a later assembly switch can load it from offset 0 of the
 * TCB. stack points at the lowest word of the caller-provided buffer.
 * The buffer itself is not owned or freed here; allocation is static and
 * remains the caller's responsibility.
 */
typedef struct litert_tcb {
    uint32_t *sp;
    uint32_t *stack;
    uint32_t stack_words;
} litert_tcb_t;

/*
 * Build an initial frame at the high end of [stack, stack + stack_words).
 *
 * entry_pc  Address of the task's first instruction. Bit 0 is cleared
 *           because a C function pointer on Thumb has that bit set, while
 *           the stacked PC must be halfword-aligned. Thumb state is
 *           recorded in xPSR, not in the PC slot.
 * arg0      Value restored into R0. For void task(void *arg) this is arg.
 * exit_lr   Value restored into LR. Stored unchanged, including bit 0:
 *           unlike the stacked PC, LR bit 0 is the Thumb interworking bit
 *           the compiler uses on a normal return. There is no exit handler
 *           in this increment; the caller chooses the placeholder.
 *
 * Preconditions:
 *   - stack is 8-byte aligned (use _Alignas(8)). Cortex-M exception entry
 *     forces doubleword alignment, and AAPCS requires SP 8-byte aligned
 *     at a function boundary. The frame is 16 words (64 bytes), so an
 *     aligned top produces an aligned PSP.
 *   - stack_words is even, so the top stays 8-byte aligned, and at least
 *     LITERT_FRAME_WORDS.
 *
 * On success, tcb->sp points at the R4 slot. Words below the frame are
 * left untouched so a caller can paint them and later see how deep the
 * task actually used the stack. On failure, *tcb and the buffer are
 * unchanged.
 */
litert_status_t litert_task_init(litert_tcb_t *tcb,
                                 uint32_t *stack,
                                 uint32_t stack_words,
                                 uint32_t entry_pc,
                                 uint32_t arg0,
                                 uint32_t exit_lr);

#endif /* LITERT_TASK_H */
