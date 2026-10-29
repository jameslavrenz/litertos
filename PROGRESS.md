# LiteRTOS progress

Educational kernel. One increment at a time. Do not skip ahead.

How to build and run the checks is in `README.md`. What the five steps mean is in `docs/steps.md`. Design notes live in `docs/implementation.md`. Update that file at the end of every increment.

## Phase 1 — QEMU (mps2-an386)

| Step | Status | What exists |
| --- | --- | --- |
| 1. Task creation and stack initialization | **done** | TCB, Cortex-M initial stack frame, host test |
| 2. Cooperative multitasking | **done** | SVC start and yield, two tasks on mps2-an386 |
| 3. Cooperative scheduler | not started | Three tasks, ready list, priorities |
| 4. Preemptive multitasking | not started | SysTick, PendSV, fixed-priority preemption |
| 5. Kernel refinement | not started | States, delays, critical sections, semaphores, mutexes, faults, tests |

## Phase 2 — STM32F446 Nucleo

Not started. Shared kernel code stays target-agnostic; startup and the linker script come later.

## How to test

```sh
make test    # Step 1 stack frame, host compiler
make qemu    # Step 2, two tasks on mps2-an386
```
