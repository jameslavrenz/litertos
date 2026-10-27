/*
 * Cortex-M initial stack frame.
 *
 * Pure memory writes. No MSR/MRS, no exception, no PendSV. The goal is to
 * leave memory in the shape those instructions will expect later.
 */
#include "litert/task.h"

#include <stddef.h>

_Static_assert(LITERT_FRAME_WORDS == 16, "integer frame is 8 software + 8 hardware words");
_Static_assert(LITERT_FRAME_XPSR == LITERT_FRAME_WORDS - 1, "xPSR occupies the highest slot");
_Static_assert(sizeof(uint32_t) == 4, "a Cortex-M register image is 4 bytes");

litert_status_t litert_task_init(litert_tcb_t *tcb,
                                 uint32_t *stack,
                                 uint32_t stack_words,
                                 uint32_t entry_pc,
                                 uint32_t arg0,
                                 uint32_t exit_lr)
{
    uint32_t *top;
    uint32_t *frame;

    if (tcb == NULL || stack == NULL) {
        return LITERT_ERR_PARAM;
    }
    if (stack_words < (uint32_t)LITERT_FRAME_WORDS) {
        return LITERT_ERR_NO_STACK;
    }
    /*
     * 8-byte alignment of the base, plus an even word count, makes the
     * top 8-byte aligned. Reject rather than silently rounding so a
     * mis-sized static buffer fails at task creation instead of at the
     * first exception return (UsageFault, UNALIGNED or INVSTATE).
     */
    if (((uintptr_t)stack & 7u) != 0u || (stack_words & 1u) != 0u) {
        return LITERT_ERR_ALIGN;
    }

    /* One past the last word: the SP the task will see on entry. */
    top = stack + stack_words;
    frame = top - LITERT_FRAME_WORDS;

    frame[LITERT_FRAME_R4] = 0u;
    frame[LITERT_FRAME_R5] = 0u;
    frame[LITERT_FRAME_R6] = 0u;
    frame[LITERT_FRAME_R7] = 0u;
    frame[LITERT_FRAME_R8] = 0u;
    frame[LITERT_FRAME_R9] = 0u;
    frame[LITERT_FRAME_R10] = 0u;
    frame[LITERT_FRAME_R11] = 0u;

    frame[LITERT_FRAME_R0] = arg0;
    frame[LITERT_FRAME_R1] = 0u;
    frame[LITERT_FRAME_R2] = 0u;
    frame[LITERT_FRAME_R3] = 0u;
    frame[LITERT_FRAME_R12] = 0u;
    frame[LITERT_FRAME_LR] = exit_lr;
    frame[LITERT_FRAME_PC] = entry_pc & LITERT_PC_THUMB_MASK;
    frame[LITERT_FRAME_XPSR] = LITERT_INITIAL_XPSR;

    tcb->sp = frame;
    tcb->stack = stack;
    tcb->stack_words = stack_words;
    return LITERT_OK;
}
