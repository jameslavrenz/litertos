# Host build for architecture-independent tests.
# Cross-compilation for QEMU (arm-none-eabi) is a later increment.
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow -Iinclude
BUILD   := build

.PHONY: all test clean

all: $(BUILD)/test_task_stack

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/task_stack.o: src/arch/cortex_m/task_stack.c include/litert/task.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/arch/cortex_m/task_stack.c -o $@

$(BUILD)/test_task_stack: tests/test_task_stack.c $(BUILD)/task_stack.o
	$(CC) $(CFLAGS) tests/test_task_stack.c $(BUILD)/task_stack.o -o $@

test: $(BUILD)/test_task_stack
	./$(BUILD)/test_task_stack

clean:
	rm -rf $(BUILD)
