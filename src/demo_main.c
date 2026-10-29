/*
 * Phase 1 — QEMU mps2-an386. Not part of the kernel. Phase 2 replaces the
 * UART address; the two worker tasks can stay.
 * Step 2 — Demo of cooperative yield.
 *
 * Two tasks, one function. Each has its own stack, so the loop counter
 * survives a yield. UART0 is the mps2-an386 CMSDK UART at 0x40004000.
 * Step 3's demo will add a third task. Step 4's demo will show a tick
 * preempting a task that does not yield. Step 5's demo will block.
 */
#include "litert/sched.h"

#include <stdint.h>

#define UART0_BASE 0x40004000u

enum {
    DEMO_STACK_WORDS = 128
};

static volatile uint32_t *const uart0 = (volatile uint32_t *)UART0_BASE;

static void uart_init(void)
{
    uart0[4] = 16u; /* BAUDDIV, minimum value the model accepts */
    uart0[2] = 1u;  /* CTRL.TX_EN */
}

static void uart_putc(unsigned char c)
{
    while ((uart0[1] & 1u) != 0u) {
        /* STATE.TXFULL */
    }
    uart0[0] = (uint32_t)c;
}

static void uart_puts(const char *s)
{
    while (*s != '\0') {
        uart_putc((unsigned char)*s);
        s++;
    }
}

void qemu_exit_ok(void);

static void worker(void *arg)
{
    const char *name = (const char *)arg;
    int i;

    for (i = 0; i < 4; i++) {
        uart_putc((unsigned char)name[0]);
        uart_putc((unsigned char)('0' + i));
        uart_putc((unsigned char)'\n');
        if (name[0] == 'B' && i == 3) {
            uart_puts("done\n");
            qemu_exit_ok();
        }
        litert_yield();
    }
}

/* Phase 1 Step 2 stand-in. Step 5 replaces this with kernel fault handling. */
void HardFault_Handler(void)
{
    uart_putc((unsigned char)'H');
    uart_putc((unsigned char)'\n');
    for (;;) {
    }
}

int main(void)
{
    static _Alignas(8) uint32_t stack_a[DEMO_STACK_WORDS];
    static _Alignas(8) uint32_t stack_b[DEMO_STACK_WORDS];
    static litert_tcb_t task_a;
    static litert_tcb_t task_b;
    static litert_tcb_t rejected;
    static const char name_a[] = "A";
    static const char name_b[] = "B";

    uart_init();

    if (litert_task_create(&task_a, stack_a, DEMO_STACK_WORDS, worker, (void *)name_a) != LITERT_OK ||
        litert_task_create(&task_b, stack_b, DEMO_STACK_WORDS, worker, (void *)name_b) != LITERT_OK) {
        uart_putc((unsigned char)'E');
        for (;;) {
        }
    }
    /* Step 2 is exactly two tasks. A third create must fail before start. */
    if (litert_task_create(&rejected, stack_a, DEMO_STACK_WORDS, worker, (void *)name_a) != LITERT_ERR_FULL) {
        uart_putc((unsigned char)'E');
        for (;;) {
        }
    }

    litert_start();
    return 0;
}
