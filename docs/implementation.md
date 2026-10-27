# LiteRTOS implementation notes

Running record of the kernel. **Step 1 is done.** Steps 2–5 are not started. The concept map for the five steps is `docs/steps.md`.

Update this file at the end of every increment, before stopping. Add what changed, why, how it was tested, and one or two experiments. Do not leave a completed step described only in chat. Keep `PROGRESS.md` in sync with the status table below.

One increment at a time. Do not implement the next step while updating this document.

## Status

| Step | Scope | Status |
| --- | --- | --- |
| 1 | Task creation and stack initialization | **done** |
| 2 | Cooperative multitasking | not started |
| 3 | Cooperative scheduler | not started |
| 4 | Preemptive multitasking | not started |
| 5 | Kernel refinement | not started |
| Phase 2 | STM32F446 Nucleo | not started |

QEMU is not used in Step 1. It starts in Step 2, when the first task is launched, and stays the execution environment through Step 5. The physical board is Phase 2.

There is no assembly yet. `litert_task_init()` only writes `uint32_t` slots. Loading the PSP, writing `CONTROL`, and returning through `EXC_RETURN` are Step 2.

## Step 1 — Task creation and stack initialization

### What was built

A task control block and a fabricated Cortex-M stack frame. Nothing switches tasks, and nothing boots QEMU. The frame is the memory image a later context restore will pop, written in advance because a new task has never taken an exception.

### Files

| File | Purpose |
| --- | --- |
| `include/litert/task.h` | TCB, frame indices, and the init contract |
| `src/arch/cortex_m/task_stack.c` | Cortex-M frame construction. The only architecture-specific code so far |
| `tests/test_task_stack.c` | Host checks of layout, alignment, endianness, and rejection |
| `Makefile` | Host `cc` build. No cross toolchain yet |

The kernel sources are about 210 physical lines, of which about 88 are declarations or executable code. The rest is the frame contract in comments.

### Task control block

```c
typedef struct litert_tcb {
    uint32_t *sp;          /* first field: future assembly loads it at offset 0 */
    uint32_t *stack;       /* lowest word of the caller-provided buffer */
    uint32_t stack_words;
} litert_tcb_t;
```

`sp` is first so a later assembly switch can load it with one word load from the start of the TCB. The buffer is caller-owned and statically allocated. This function does not allocate or free it.

`litert_task_init()` takes three register images, all `uint32_t`:

| Argument | Stacked into | Meaning |
| --- | --- | --- |
| `entry_pc` | PC | Address of the task's first instruction. Bit 0 is cleared. |
| `arg0` | R0 | First argument under AAPCS. For `void task(void *arg)` this is `arg`. |
| `exit_lr` | LR | Where execution goes if the task function returns. Stored unchanged, including bit 0. No exit handler exists yet; the caller picks the placeholder. |

On success, `tcb->sp` points at the R4 slot. Words below the frame are left untouched so a caller can paint them and later measure stack use. On failure, the TCB and the buffer are unchanged.

| Return | Cause |
| --- | --- |
| `LITERT_OK` | Frame written |
| `LITERT_ERR_PARAM` | NULL TCB or NULL stack |
| `LITERT_ERR_NO_STACK` | `stack_words < 16` |
| `LITERT_ERR_ALIGN` | Base is not 8-byte aligned, or `stack_words` is odd |

Misalignment is rejected rather than silently rounded. A bad static buffer should fail at creation, not on the first exception return.

### MSP and PSP

Cortex-M has two stack pointers. In Thread mode, `CONTROL.SPSEL` selects which one `SP` aliases. Handler mode always uses the Main Stack Pointer (MSP): reset, faults, SysTick, and PendSV do their own work there.

Task stacks use the Process Stack Pointer (PSP). The exception frame then lands on the task stack, and the handler keeps using MSP. A task that overruns its stack does not immediately corrupt the kernel stack.

`litert_task_init()` does not write `CONTROL` or either stack pointer. It only prepares the memory the future PSP will point at. At reset the core is in privileged Thread mode with `SPSEL` clear, so `SP` is still MSP.

### The initial stack frame

The frame matches two pushes that have not happened yet:

1. Hardware exception entry, which always stacks eight words: R0, R1, R2, R3, R12, LR, PC, xPSR. That order is fixed by ARMv7-M.
2. A later software save of the callee-saved registers with `STMDB sp!, {r4-r11}`. `STMDB` stores the lowest-numbered register at the lowest address, so R4 is at the future PSP and R11 is just below the hardware frame.

The restore contract, not implemented yet, is the inverse: `LDMIA sp!, {r4-r11}`, then `BX LR` with an `EXC_RETURN` value that tells the processor to unstack from the PSP. `EXC_RETURN` is not stored in this frame. It will live in the handler's LR.

```
higher addresses
+------------------+
| xPSR             |  [15]  0x01000000   EPSR.T set
| PC               |  [14]  task entry, bit 0 clear
| LR               |  [13]  taken if the task function returns
| R12              |  [12]
| R3               |  [11]
| R2               |  [10]
| R1               |  [9]
| R0               |  [8]   task argument (AAPCS)
+------------------+  8 words, hardware frame
| R11              |  [7]
| R10              |  [6]
| R9               |  [5]
| R8               |  [4]
| R7               |  [3]
| R6               |  [2]
| R5               |  [1]
| R4               |  [0]   <-- tcb->sp, future PSP
+------------------+  8 words, software frame
lower addresses     unused stack, left as the caller painted it
```

Indices match `LITERT_FRAME_*` in `include/litert/task.h`. The frame is 16 words, 64 bytes. R4–R11, R1–R3, and R12 are written as zero. Incoming callee-saved registers are unused at a C function entry, and zero keeps the image deterministic. A pattern such as `0x04040404` in R4 would make a later dump easier to read, and would also look like a live value.

Pointer construction in `src/arch/cortex_m/task_stack.c`:

```c
top = stack + stack_words;          /* one past the last word; never written */
frame = top - LITERT_FRAME_WORDS;   /* 16 words down; frame[15] is xPSR */
tcb->sp = frame;
```

`top` is the SP the task function will see after both the software pop and the hardware pop.

### PC, LR, and xPSR

**xPSR = `0x01000000`.** Bit 24 is EPSR.T. Cortex-M has no ARM instruction state. Returning with T clear raises UsageFault `INVSTATE`. The condition flags, the Q bit, bit 9 (alignment adjustment), and the IPSR exception number are zero. IPSR zero means the fabricated return is to thread context.

**PC.** A Thumb function pointer has bit 0 set. Real exception entry stacks a halfword-aligned return address and keeps Thumb state in EPSR.T. Bit 0 of `entry_pc` is cleared with `LITERT_PC_THUMB_MASK` (`0xFFFFFFFE`). The host test passes `0x08000101` and expects `0x08000100`.

**LR.** This slot is restored into LR as data, not used as the exception-return code. Bit 0 is the interworking bit a later `BX LR` would honor, so `exit_lr` is stored unchanged. The test stores `0x0800ABCD`, then checks that a later call with LR equal to `1` keeps that bit.

### Alignment

`CCR.STKALIGN` defaults to 1 on Cortex-M4, so a real exception entry forces 8-byte stack alignment and records a 4-byte adjustment in xPSR bit 9. This frame sets bit 9 to 0 and does not insert a padding word above xPSR. Setting bit 9 without that padding would shift the task's SP by 4 bytes for its whole life.

Callers must pass an 8-byte-aligned buffer (`_Alignas(8)`) and an even word count. The frame is a multiple of 8 bytes, so an aligned top produces an aligned PSP. After both pops, the task function's SP is that same top, which is what AAPCS requires at a function boundary.

### Host pointers and Cortex-M addresses

Frame slots are `uint32_t` because a Cortex-M register is 32 bits. On this Mac a function pointer is 8 bytes, so `entry_pc` and `exit_lr` are 32-bit constants, not function pointers. When the same function is later compiled for Cortex-M, a function pointer is already 32 bits.

The TCB's `sp` is a C pointer to those slots. It is 8 bytes in the host test and will be 4 bytes on the target. The values inside the frame are never host pointers. The test checks that adjacent slots are 4 bytes apart.

Apple Silicon is little-endian, like Cortex-M. The test checks that xPSR `0x01000000` is stored as the bytes `00 00 00 01`.

No floating-point context is stacked. An extended frame would add S0–S15 and FPSCR. This port assumes the software floating-point ABI and `CONTROL.FPCA` clear.

### How it was tested

```sh
make test
```

That compiles with the host compiler and runs `build/test_task_stack`. It passed. A second build with `-Wconversion` and address/undefined sanitizers also passed. QEMU was not run.

The stable part of the dump:

```
frame at 0x16d8113e8 (slot size 4 bytes, host pointer 8 bytes)
  xPSR [15]  0x01000000
  PC   [14]  0x08000100
  LR   [13]  0x0800ABCD
  R0   [ 8]  0xA0A1A2A3
  R4   [ 0]  0x00000000   <-- sp / future PSP
all task-stack checks passed
```

The frame address is a host stack pointer and changes between runs. It was 8-byte aligned. The test also checks:

- The 48 words below a 64-word painted stack stay `0xA5A5A5A5`.
- A 16-word stack is an exact fit: `tcb->sp` equals the base of the buffer.
- NULL, a 15-word stack, a pointer that is only 4-byte aligned, and an odd word count are rejected, and those failures do not write the TCB or the buffer.
- A PC whose bit 0 is already clear stays clear.

### What Step 1 should make clear

- The PSP points at R4. The hardware frame starts at R0, eight words higher.
- EPSR.T is set. The stacked PC has bit 0 clear. The stacked LR keeps bit 0.
- The frame is 64 bytes, and both the PSP and the eventual task SP are 8-byte aligned.
- A 64-bit host pointer never appears inside the frame.

### Experiments

1. In `litert_task_init()`, clear xPSR bit 24 and watch `make test` fail. The fault on a real exception return would be UsageFault `INVSTATE`, caused by EPSR.T being clear.
2. Paint a stack with `0xA5A5A5A5`, init a task with a 128-word buffer, and count the bytes left for the task's own locals (112 words, 448 bytes). Then set `exit_lr` to an odd address and confirm the PC slot does not gain a Thumb bit.

### Explicitly not in this step

No scheduler, no ready list, no SysTick, no PendSV, no task switch, no QEMU image, no startup code, no linker script, no assembly.

### Next increment

Step 2 begins by starting a single task under QEMU `mps2-an386`: startup code, a vector table, a linker script, load PSP from `tcb->sp`, and exception-return into that one task. No yield and no second task. The two-task switch is the following increment, still inside Step 2.

The future first-task launch, not yet code, looks like this:

```asm
ldmia r0!, {r4-r11}    ; pop the software frame; r0 advances to the R0 slot
msr   psp, r0          ; hardware unstack starts here
mov   lr, #0xFFFFFFFD  ; EXC_RETURN: Thread mode, restore from PSP
bx    lr               ; hardware pops R0–xPSR and jumps to the stacked PC
```

`0xFFFFFFFD` is `EXC_RETURN` in the handler LR. It is not the LR value stored in the frame.

## Step 2 — Cooperative multitasking

Not started. When this step is implemented, replace this section with:

- how the first task is started, including the PSP and `EXC_RETURN` sequence
- the assembly that saves and restores a context, and the register order it must share with Step 1
- how explicit yield switches two tasks
- the QEMU command, what was observed, and experiments

## Step 3 — Cooperative scheduler

Not started. Document ready-task management, priority selection, and how starvation shows up under QEMU.

## Step 4 — Preemptive multitasking

Not started. Document SysTick, PendSV, exception entry and return, which registers hardware saves, and which registers software saves.

## Step 5 — Kernel refinement

Not started. Add one subsection per feature as it lands: task states, delays, critical sections, semaphores, mutexes, priority inversion, faults, and automated tests.

## Phase 2 — STM32F446

Not started. Document what stayed shared with the QEMU port, the startup and linker changes, and any measured switch latency.
