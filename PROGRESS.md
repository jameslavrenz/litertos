# LiteRTOS progress

Educational kernel. One increment at a time. Do not skip ahead.

## Phase 1 — QEMU (mps2-an386)

| Step | Status | What exists |
| --- | --- | --- |
| 1. Task creation and stack initialization | **done** | TCB, Cortex-M initial stack frame, host test |
| 2. Cooperative multitasking | not started | Start the first task, explicit yield, two-task switch |
| 3. Cooperative scheduler | not started | Three tasks, ready list, priorities |
| 4. Preemptive multitasking | not started | SysTick, PendSV, fixed-priority preemption |
| 5. Kernel refinement | not started | States, delays, critical sections, semaphores, mutexes, faults, tests |

## Phase 2 — STM32F446 Nucleo

Not started. Shared kernel code stays target-agnostic; startup and the linker script come later.

## How to test step 1

```sh
make test
```

Host compiler only. No QEMU image yet.
