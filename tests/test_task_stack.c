/*
 * Host-side check of the Cortex-M initial stack frame.
 *
 * The frame is an array of uint32_t register images. On this 64-bit Mac
 * a pointer does not fit in a slot, so entry_pc and exit_lr are plain
 * 32-bit constants, not function pointers. Apple Silicon is little-endian,
 * matching Cortex-M, so the byte-order check below is meaningful here.
 */
#include "litert/task.h"

#include <stdint.h>
#include <stdio.h>

static int g_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

static const char *const k_frame_names[LITERT_FRAME_WORDS] = {
    "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11",
    "R0", "R1", "R2", "R3", "R12", "LR", "PC", "xPSR",
};

static void dump_frame(const litert_tcb_t *tcb)
{
    int i;

    printf("frame at %p (slot size %u bytes, host pointer %u bytes)\n",
           (void *)tcb->sp,
           (unsigned)sizeof(tcb->sp[0]),
           (unsigned)sizeof(void *));
    for (i = LITERT_FRAME_WORDS - 1; i >= 0; i--) {
        printf("  %s [%2d]  0x%08X%s\n",
               k_frame_names[i],
               i,
               tcb->sp[i],
               i == 0 ? "   <-- sp / future PSP" : "");
    }
}

static void test_pointer_width(void)
{
    /*
     * This test binary is built for the host. The frame must stay 32-bit
     * wide even when void* is 64-bit, which is the usual macOS ABI.
     */
    CHECK(sizeof(uint32_t) == 4u);
    CHECK(sizeof(((litert_tcb_t *)0)->sp[0]) == 4u);
#if UINTPTR_MAX > 0xFFFFFFFFu
    CHECK(sizeof(void *) == 8u);
    CHECK(sizeof(void *) > sizeof(uint32_t));
#else
    CHECK(sizeof(void *) == 4u);
#endif
}

static void test_initial_frame(void)
{
    enum { k_words = 64 };
    _Alignas(8) uint32_t stack[k_words];
    litert_tcb_t tcb;
    litert_status_t status;
    const uint32_t entry_pc = 0x08000101u; /* Thumb bit set, as a C pointer would be */
    const uint32_t arg0 = 0xA0A1A2A3u;
    const uint32_t exit_lr = 0x0800ABCDu;  /* bit 0 clear on purpose: stored as-is */
    const uint32_t paint = 0xA5A5A5A5u;
    unsigned char *xpsr_bytes;
    uint32_t i;

    for (i = 0; i < k_words; i++) {
        stack[i] = paint;
    }

    status = litert_task_init(&tcb, stack, k_words, entry_pc, arg0, exit_lr);
    CHECK(status == LITERT_OK);
    if (status != LITERT_OK) {
        return;
    }

    CHECK(tcb.stack == stack);
    CHECK(tcb.stack_words == (uint32_t)k_words);
    CHECK(tcb.sp == stack + (k_words - LITERT_FRAME_WORDS));
    CHECK(((uintptr_t)tcb.sp & 7u) == 0u);
    /* SP the task function sees after both the software and hardware pops. */
    CHECK(((uintptr_t)(tcb.sp + LITERT_FRAME_WORDS) & 7u) == 0u);
    CHECK(tcb.sp + LITERT_FRAME_WORDS == stack + k_words);

    CHECK(tcb.sp[LITERT_FRAME_R4] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R5] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R6] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R7] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R8] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R9] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R10] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R11] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R0] == arg0);
    CHECK(tcb.sp[LITERT_FRAME_R1] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R2] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R3] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_R12] == 0u);
    CHECK(tcb.sp[LITERT_FRAME_LR] == exit_lr);
    CHECK(tcb.sp[LITERT_FRAME_PC] == 0x08000100u);
    CHECK(tcb.sp[LITERT_FRAME_XPSR] == LITERT_INITIAL_XPSR);
    CHECK((tcb.sp[LITERT_FRAME_XPSR] & (1u << 24)) != 0u);

    /* Adjacent slots are 4 bytes apart, not a host pointer apart. */
    CHECK((uintptr_t)&tcb.sp[LITERT_FRAME_PC] - (uintptr_t)&tcb.sp[LITERT_FRAME_LR] == 4u);
    CHECK((uintptr_t)&tcb.sp[LITERT_FRAME_XPSR] - (uintptr_t)&tcb.sp[LITERT_FRAME_R4] ==
          (LITERT_FRAME_WORDS - 1) * 4u);

    /* Little-endian memory image of xPSR 0x01000000, matching Cortex-M. */
    xpsr_bytes = (unsigned char *)&tcb.sp[LITERT_FRAME_XPSR];
    CHECK(xpsr_bytes[0] == 0x00u);
    CHECK(xpsr_bytes[1] == 0x00u);
    CHECK(xpsr_bytes[2] == 0x00u);
    CHECK(xpsr_bytes[3] == 0x01u);

    for (i = 0; i < (uint32_t)(k_words - LITERT_FRAME_WORDS); i++) {
        CHECK(stack[i] == paint);
    }

    dump_frame(&tcb);

    /* Bit 0 already clear stays clear. LR bit 0 is stored, not stripped. */
    status = litert_task_init(&tcb, stack, k_words, 0x20000000u, 0u, 1u);
    CHECK(status == LITERT_OK);
    CHECK(tcb.sp[LITERT_FRAME_PC] == 0x20000000u);
    CHECK(tcb.sp[LITERT_FRAME_LR] == 1u);
}

static void test_exact_fit(void)
{
    _Alignas(8) uint32_t stack[LITERT_FRAME_WORDS];
    litert_tcb_t tcb;
    litert_status_t status;

    status = litert_task_init(&tcb, stack, LITERT_FRAME_WORDS, 0x1000u, 0x42u, 0u);
    CHECK(status == LITERT_OK);
    CHECK(tcb.sp == stack);
    CHECK(tcb.sp[LITERT_FRAME_R0] == 0x42u);
    CHECK(stack[LITERT_FRAME_XPSR] == LITERT_INITIAL_XPSR);
}

static void test_rejects(void)
{
    _Alignas(8) uint32_t aligned[32];
    litert_tcb_t tcb;
    litert_tcb_t original;
    uint32_t i;
    const uint32_t paint = 0xA5A5A5A5u;

    for (i = 0; i < 32u; i++) {
        aligned[i] = paint;
    }
    tcb.sp = (uint32_t *)(uintptr_t)0x1111u;
    tcb.stack = (uint32_t *)(uintptr_t)0x2222u;
    tcb.stack_words = 0x3333u;
    original = tcb;

    CHECK(litert_task_init(NULL, aligned, 32u, 0u, 0u, 0u) == LITERT_ERR_PARAM);
    CHECK(litert_task_init(&tcb, NULL, 32u, 0u, 0u, 0u) == LITERT_ERR_PARAM);
    CHECK(tcb.sp == original.sp);
    CHECK(tcb.stack == original.stack);
    CHECK(tcb.stack_words == original.stack_words);

    CHECK(litert_task_init(&tcb, aligned, LITERT_FRAME_WORDS - 1u, 0u, 0u, 0u) ==
          LITERT_ERR_NO_STACK);
    CHECK(tcb.sp == original.sp);

    /* One word past an 8-byte-aligned base is only 4-byte aligned. */
    CHECK(litert_task_init(&tcb, aligned + 1, 16u, 0u, 0u, 0u) == LITERT_ERR_ALIGN);
    CHECK(tcb.sp == original.sp);

    /* Even base, odd length: the top is only 4-byte aligned. */
    CHECK(litert_task_init(&tcb, aligned, 17u, 0u, 0u, 0u) == LITERT_ERR_ALIGN);
    CHECK(tcb.sp == original.sp);

    for (i = 0; i < 32u; i++) {
        CHECK(aligned[i] == paint);
    }
}

int main(void)
{
    test_pointer_width();
    test_initial_frame();
    test_exact_fit();
    test_rejects();

    if (g_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all task-stack checks passed\n");
    return 0;
}
