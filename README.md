# LiteRTOS

A small teaching real-time kernel for ARM Cortex-M4. It is written from scratch so each scheduling mechanism stays visible. FreeRTOS, Zephyr, and other existing kernels are not used.

The firmware prints text on a serial port. That text is how you see what the core did. There is no graphical interface.

## Status

Phase 1 runs under QEMU's `mps2-an386` Cortex-M4 board. Phase 2, later, is a physical STM32F446 Nucleo. The kernel sources are shared. Phase 2 replaces startup code, the linker script, and the demo UART.

| Step | What it adds | Status |
| --- | --- | --- |
| 1 | Task control block and initial stack frame | done |
| 2 | Start two tasks and switch when one yields | done |
| 3 | Three tasks, a ready set, and priority | not started |
| 4 | SysTick preemption through PendSV | not started |
| 5 | Delays, semaphores, mutexes, faults | not started |

`docs/steps.md` explains the ideas and how the steps differ. `docs/implementation.md` records what has been built, including register layouts and test results. `PROGRESS.md` is the short status list. Source files name their phase and step in the first comment.

## Requirements

- A C11 compiler for the host tests (`cc` on macOS)
- `arm-none-eabi-gcc` for the QEMU image
- `qemu-system-arm` with the `mps2-an386` machine
- Python 3, used only to launch QEMU and check the transcript

## Run the checks

From this directory:

```sh
make test    # Step 1. Host program. Checks the stack frame in memory.
make qemu    # Step 2. Boots mps2-an386 and switches two tasks.
```

`make test` does not start QEMU. It compiles `tests/test_task_stack.c` with the Mac compiler.

`make qemu` builds `build/litert.elf` and runs:

```sh
qemu-system-arm -machine mps2-an386 -kernel build/litert.elf \
  -display none -monitor none -serial stdio \
  -semihosting-config enable=on,target=native
```

`-display none` keeps QEMU in the terminal. A passing Step 2 run prints:

```text
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

`A` is the first task and `B` is the second. They run the same function. The digit is that task's own loop counter. `A0` followed by `B0` followed by `A1` means a yield switched tasks and each stack was restored. `done` is the demo stopping QEMU through semihosting.

To watch the board output without the checker, run the `qemu-system-arm` command above after `make qemu` has produced `build/litert.elf`.

## Layout

| Path | Role |
| --- | --- |
| `include/litert/task.h` | Step 1. Task control block and stack-frame contract |
| `src/arch/cortex_m/task_stack.c` | Step 1. Fills a new task's stack |
| `include/litert/sched.h`, `src/sched.c` | Step 2. Create two tasks and choose the other one on yield |
| `src/arch/cortex_m/context.s` | Step 2. `SVC` save and restore |
| `src/arch/cortex_m/startup.s` | Phase 1 startup and vector table |
| `src/demo_main.c` | Phase 1 demo. The `A` / `B` lines |
| `linker/mps2-an386.ld` | Phase 1 memory map. Phase 2 gets its own script |
| `tests/test_task_stack.c` | Step 1 host test |

The image is built with `-mfloat-abi=soft`. Task context does not include floating-point registers.
