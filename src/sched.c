/*
 * Phase 1 — QEMU mps2-an386. Shared with Phase 2 (STM32F446): yes.
 * Step 2 — Cooperative multitasking.
 *
 * context.s saves and restores registers. This file only chooses which
 * TCB runs next. With two tasks that choice is "the other one."
 *
 *   Step 3 replaces litert_switch_next() with priority selection.
 *   Step 4 calls that same function from PendSV; this file stays portable.
 *   Step 5 makes selection skip blocked tasks.
 */
#include "litert/sched.h"

#include <stddef.h>

_Static_assert(sizeof(void *) == 4, "Cortex-M stores a task entry in one register");
_Static_assert(offsetof(litert_tcb_t, sp) == 0,
               "context.s loads the stack pointer from the first TCB word");

/* Read and written by SVC_Handler. sp of the task that will be restored. */
litert_tcb_t *litert_current;

/* 0 until the first SVC. The handler must not save main's registers. */
uint32_t litert_running;

static litert_tcb_t *slots[LITERT_MAX_TASKS];
static uint32_t task_count;

/*
 * A task that returns lands here because this address, Thumb bit set,
 * was stored in its initial LR. There is no exit code yet.
 */
void litert_task_returned(void)
{
    for (;;) {
    }
}

litert_status_t litert_task_create(litert_tcb_t *tcb,
                                   uint32_t *stack,
                                   uint32_t stack_words,
                                   litert_task_fn entry,
                                   void *arg)
{
    litert_status_t status;

    if (tcb == NULL || stack == NULL || entry == NULL) {
        return LITERT_ERR_PARAM;
    }
    if (task_count >= (uint32_t)LITERT_MAX_TASKS) {
        return LITERT_ERR_FULL;
    }

    status = litert_task_init(tcb,
                              stack,
                              stack_words,
                              (uint32_t)(uintptr_t)entry,
                              (uint32_t)(uintptr_t)arg,
                              (uint32_t)(uintptr_t)litert_task_returned);
    if (status != LITERT_OK) {
        return status;
    }

    slots[task_count] = tcb;
    task_count++;
    if (litert_current == NULL) {
        litert_current = tcb;
    }
    return LITERT_OK;
}

/*
 * Phase 1, Step 2 policy. Called from SVC_Handler with the current task
 * already saved. Returns the TCB to restore and updates litert_current.
 *
 * Step 3 replaces this body: pick the highest-priority ready task.
 * Step 4 calls the replacement from PendSV as well as from SVC.
 * Step 5 must not return a blocked task.
 */
litert_tcb_t *litert_switch_next(void)
{
    if (litert_current == slots[0]) {
        litert_current = slots[1];
    } else {
        litert_current = slots[0];
    }
    return litert_current;
}

void litert_start(void)
{
    if (task_count != (uint32_t)LITERT_MAX_TASKS) {
        for (;;) {
        }
    }
    /*
     * The SVC inside litert_yield does not return on this first call.
     * The handler sees litert_running == 0 and exception-returns into
     * the first task instead of resuming this function.
     */
    litert_yield();
    for (;;) {
    }
}
