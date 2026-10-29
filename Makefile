# Phase 1. Step 1 is the host test (make test). Step 2 is the mps2-an386
# image (make qemu). Steps 3–5 extend that same image. Phase 2 adds a
# second link of the kernel, aimed at the STM32F446, and does not remove
# these targets.
# Host tests use the Mac compiler. The QEMU image uses arm-none-eabi-gcc.
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Iinclude
BUILD   := build

ARM_CC      ?= arm-none-eabi-gcc
ARM_CPU     := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
ARM_CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Wconversion \
               $(ARM_CPU) -ffreestanding -fno-builtin \
               -fno-unwind-tables -fno-asynchronous-unwind-tables -Iinclude
ARM_LDFLAGS := -nostdlib -T linker/mps2-an386.ld $(ARM_CPU) \
               -Wl,-Map=$(BUILD)/litert.map

.PHONY: all test qemu clean

all: $(BUILD)/test_task_stack

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/task_stack.o: src/arch/cortex_m/task_stack.c include/litert/task.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/arch/cortex_m/task_stack.c -o $@

$(BUILD)/test_task_stack: tests/test_task_stack.c $(BUILD)/task_stack.o
	$(CC) $(CFLAGS) tests/test_task_stack.c $(BUILD)/task_stack.o -o $@

test: $(BUILD)/test_task_stack
	./$(BUILD)/test_task_stack

$(BUILD)/litert.elf: src/arch/cortex_m/startup.s src/arch/cortex_m/context.s \
                     src/arch/cortex_m/task_stack.c src/sched.c src/demo_main.c \
                     src/qemu_exit.s include/litert/task.h include/litert/sched.h \
                     linker/mps2-an386.ld | $(BUILD)
	$(ARM_CC) $(ARM_CFLAGS) $(ARM_LDFLAGS) \
	    src/arch/cortex_m/startup.s src/arch/cortex_m/context.s src/qemu_exit.s \
	    src/arch/cortex_m/task_stack.c src/sched.c src/demo_main.c \
	    -lgcc -o $@

qemu: $(BUILD)/litert.elf
	python3 scripts/run_qemu.py $(BUILD)/litert.elf

clean:
	rm -rf $(BUILD)
