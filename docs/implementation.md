# LiteRTOS implementation notes

Running record of the kernel. **Steps 1 and 2 are done.** Steps 3–5 are not started. The concept map for the five steps is `docs/steps.md`.

Update this file at the end of every increment, before stopping. Add what changed, why, how it was tested, and one or two experiments. Do not leave a completed step described only in chat. Keep `PROGRESS.md` in sync with the status table below.

One increment at a time. Do not implement the next step while updating this document.

Source files name their phase and step in the first comment. Phase 1 is QEMU `mps2-an386`. Phase 2 is the STM32F446, which keeps the kernel files and replaces startup, the linker script, and the demo UART. Step 3's seam is `litert_switch_next()`. Step 4's seams are `SysTick_Handler` and `PendSV_Handler`. Step 5's seams are task state on the TCB and `HardFault_Handler`.

## Status

| Step | Scope | Status |
| --- | --- | --- |
| 1 | Task creation and stack initialization | **done** |
| 2 | Cooperative multitasking | **done** |
| 3 | Cooperative scheduler | not started |
| 4 | Preemptive multitasking | not started |
| 5 | Kernel refinement | not started |
| Phase 2 | STM32F446 Nucleo | not started |

QEMU is not used in Step 1. Step 2 runs on `mps2-an386`, and that machine stays the execution environment through Step 5. The physical board is Phase 2.

`litert_task_init()` only writes `uint32_t` slots. The assembly that loads the PSP and returns through `EXC_RETURN` is in `src/arch/cortex_m/context.s`.

## Step 1 — Task creation and stack initialization

### What was built

A task control block and a fabricated Cortex-M stack frame. Nothing switches tasks, and nothing boots QEMU. The frame is the memory image a later context restore will pop, written in advance because a new task has never taken an exception.

### Files

| File | Purpose |
| --- | --- |
| `include/litert/task.h` | TCB, frame indices, and the init contract |
| `src/arch/cortex_m/task_stack.c` | Cortex-M frame construction. Step 1's architecture-specific code |
| `tests/test_task_stack.c` | Host checks of layout, alignment, endianness, and rejection |
| `Makefile` | `make test` for this step. The QEMU image was added in Step 2 |

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

Step 2, now implemented below. The launch sequence that was only a sketch here is `SVC_Handler` in `src/arch/cortex_m/context.s`.

## Step 2 — Cooperative multitasking

### What was built

Two tasks run on QEMU's `mps2-an386` Cortex-M4. Each calls `litert_yield()` to give the core to the other. Nothing preempts. A task that never yields keeps the processor. There is no priority and no ready list. A third `litert_task_create()` returns `LITERT_ERR_FULL`.

The demo runs one function twice, with arguments `"A"` and `"B"`. The loop counter is a local, so it lives on that task's stack. The transcript alternating `A0` then `B0` then `A1` is the evidence that each stack is preserved across the switch.

### Files

| File | Purpose |
| --- | --- |
| `src/arch/cortex_m/startup.s` | Vector table, data copy, zero `.bss`, call `main` |
| `src/arch/cortex_m/context.s` | `svc #0` and `SVC_Handler`, the whole switch |
| `src/sched.c` | Create at most two tasks, and pick the other one |
| `include/litert/sched.h` | `litert_task_create`, `litert_start`, `litert_yield` |
| `src/demo_main.c` | The two workers and the UART prints |
| `src/qemu_exit.s` | Semihosting exit so QEMU stops after `done` |
| `linker/mps2-an386.ld` | Code at `0x00000000`, RAM at `0x20000000`, 4 MiB each |
| `scripts/run_qemu.py` | Runs QEMU and checks the transcript |

`task_stack.c` is unchanged. The handler consumes the frame that function builds.

### Why the first start is an SVC

`EXC_RETURN` (`0xFFFFFFFD`) is not a branch target in Thread mode. From `main`, a `bx` to that value would try to execute the address and fault. It means "leave Handler mode, return to Thread, unstack from the PSP" only when it is in LR during an exception handler.

`litert_start()` therefore calls `litert_yield()`, which is:

```asm
svc #0
bx  lr
```

That SVC is the first exception. `litert_running` is still 0, so the handler does not save `main`. It sets the flag, reloads MSP from `_estack` (`0x20400000`, the top of RAM) to drop the frame just pushed for `main`, then restores the first TCB. The `bx lr` after `svc` is not executed on this first call. Later yields do execute it: the hardware-stacked PC points at that `bx lr`, and the hardware-stacked LR points back at the task, so the task continues at the instruction after `litert_yield()`.

The handler does not write `CONTROL`. Returning with bit 2 of `EXC_RETURN` set makes the processor select the PSP and set `CONTROL.SPSEL` itself.

```
0xFFFFFFFD
  bit 4 = 1   integer stack frame, no floating-point context
  bit 3 = 1   return to Thread mode
  bit 2 = 1   unstack using the PSP
```

The assembler encodes the constant as `mvn.w lr, #2` because `~2 == 0xFFFFFFFD`. Same value, no literal pool.

### Save and restore

On a later yield the task is already in Thread mode on the PSP, so the processor pushes the hardware frame onto that task's stack before `SVC_Handler` runs. The handler body uses MSP.

```asm
mrs   r0, psp
stmdb r0!, {r4-r11}       /* R4 at the lowest address, matching Step 1 */
str   r0, [current]       /* TCB.sp is the first field */
bl    litert_switch_next  /* returns the other TCB in r0 */
ldr   r0, [r0]            /* that task's sp */
ldmia r0!, {r4-r11}
msr   psp, r0             /* now points at R0 of the hardware frame */
ldr   lr, =0xFFFFFFFD
bx    lr                  /* hardware pops R0, R1, R2, R3, R12, LR, PC, xPSR */
```

`litert_switch_next()` is the entire policy: if the current TCB is slot 0, select slot 1, and the reverse. `STMDB` of eight registers keeps the 8-byte alignment the hardware established when it stacked the exception frame.

R4–R11 are the callee-saved registers. R0–R3, R12, LR, PC, and xPSR were already stacked by the processor. That split is why the software list is only `{r4-r11}` and why it must stay in that order.

No other exception is enabled, so this handler is not re-entered. The switch is not yet guarded against a nested interrupt. PendSV, which will own the switch once a timer exists, is still a default infinite loop. SysTick is not configured.

Tasks run privileged (`CONTROL.nPRIV` stays 0). The floating-point unit is not used; the image is built with `-mfloat-abi=soft`.

If a task function returns, the initial LR (Thumb bit set) sends it to `litert_task_returned()`, which spins. The demo exits before either worker returns.

### Memory and the UART

| Region | Address | Use |
| --- | --- | --- |
| Code RAM | `0x00000000`, 4 MiB | Vector table and instructions |
| Data RAM | `0x20000000`, 4 MiB | `.data`, `.bss`, task stacks |
| MSP | `0x20400000` downward | Reset, SVC, and faults |
| UART0 | `0x40004000` | CMSDK APB UART, QEMU serial port 0 |

The demo sets `BAUDDIV` to 16 and `CTRL.TX_EN`, then writes bytes to the data register while `STATE.TXFULL` is clear. QEMU delivers those bytes to stdio.

### How it was tested

Host checks still pass:

```sh
make test
```

The board image was run with:

```sh
make qemu
```

which executes:

```sh
qemu-system-arm -machine mps2-an386 -kernel build/litert.elf \
  -display none -monitor none -serial stdio \
  -semihosting-config enable=on,target=native
```

Observed stdout, and QEMU then exited 0 because of the semihosting `BKPT 0xAB`:

```
A0
B0
A1
B1
A2
B2
A3
B3
done
mps2-an386 transcript ok
```

`A` reached `i == 3` and yielded. `B` then printed `B3` and stopped the emulator, which is why there is no further line from `A`. Each counter advanced only while its own task was running.

### Experiments

1. Comment out `litert_yield()` in `worker` and run `make qemu`. Only `A0` should appear, then the run hangs until the script times out. `B` is ready in the sense that its stack exists, but nothing ever switches to it.
2. In `litert_switch_next()`, always return `slots[0]`. Predict `A0` through `A3` and no `B` lines. The second TCB is never restored.

### Explicitly not in this step

No third task, no priority, no ready list, no SysTick, no PendSV, no blocking, no critical section. A task cannot be forced off the core.

### Next increment

Step 3: three tasks, a ready set, and priority. The switch assembly can stay. What changes is `litert_switch_next()`: choose the highest-priority ready task, and show a low-priority task starving when a higher-priority task is always ready. Still no timer.

## Step 3 — Cooperative scheduler

Not started. Document ready-task management, priority selection, and how starvation shows up under QEMU.

## Step 4 — Preemptive multitasking

Not started. Document SysTick, PendSV, exception entry and return, which registers hardware saves, and which registers software saves.

## Step 5 — Kernel refinement

Not started. Add one subsection per feature as it lands: task states, delays, critical sections, semaphores, mutexes, priority inversion, faults, and automated tests.

## Phase 2 — STM32F446

Not started. Document what stayed shared with the QEMU port, the startup and linker changes, and any measured switch latency.
