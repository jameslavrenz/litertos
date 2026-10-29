/*
 * Phase 1 — QEMU mps2-an386. Shared with Phase 2 (STM32F446): yes.
 * Step 2 — Cooperative multitasking.
 *
 * Switches only when the running task calls litert_yield(). There is no
 * timer and no priority. A third task is rejected.
 *
 *   Step 3 replaces the two-slot policy and LITERT_MAX_TASKS.
 *   Step 4 still calls yield, and also switches from PendSV on a tick.
 *   Step 5 adds block, delay, semaphore, and mutex on top of that switch.
 *   Phase 2 calls this API unchanged.
 *
 * The first call that enters the SVC handler is litert_start(). That call
 * does not return. It exception-returns into the first created task.
 * Later litert_yield() calls save the current task and restore the other.
 */
#ifndef LITERT_SCHED_H
#define LITERT_SCHED_H

#include "litert/task.h"

typedef void (*litert_task_fn)(void *arg);

/* Step 2 limit. Step 3 raises this when the ready set exists. */
#define LITERT_MAX_TASKS 2

/*
 * entry is the task function. Its Thumb bit is stripped when the PC slot
 * is written. arg is placed in the R0 slot. The stacked LR is the address
 * of a spin loop, so a task that returns does not fall off the end of memory.
 *
 * This file is compiled for Cortex-M only. A void * is a 32-bit register
 * image there. The host test does not call this function.
 */
litert_status_t litert_task_create(litert_tcb_t *tcb,
                                   uint32_t *stack,
                                   uint32_t stack_words,
                                   litert_task_fn entry,
                                   void *arg);

/* Does not return. Requires LITERT_MAX_TASKS successful creates. */
void litert_start(void);

/* Save this task and run the other one. Returns after the other task yields. */
void litert_yield(void);

#endif /* LITERT_SCHED_H */
