# The five steps

Preliminary map of LiteRTOS. It explains the real-time operating system ideas each step introduces, and how the steps differ. It describes the plan. Step 1 matches the code already in the tree. Steps 2–5 will be revised here if the design changes.

What has actually been built, including register layouts and test results, is in `docs/implementation.md`. Update that file when an increment lands. Update this file when the meaning of a step changes.

## One processor, several activities

The chip this kernel targets has one CPU core. The core executes one instruction stream. An application still has several activities that are all "in progress": read a sensor, run a control loop, wait for a command, blink a light. Those activities are written as separate **tasks**.

A task is a function with a life longer than a single call. An ordinary function call ends with a return, and its local variables disappear. A task must be able to pause in the middle and resume later with those locals and the CPU registers still intact. On a single core the tasks do not run at the same instant. They are **concurrent**: their lives overlap, and the core interleaves them. They are not parallel. Parallel would mean two cores executing two tasks at the same physical moment. This project has one core.

The piece of software that remembers the tasks and chooses which one runs is the **kernel**. A **real-time operating system** (RTOS) is a kernel whose job is to make that choice in a bounded, predictable way. "Real-time" means a high-priority task can be given the processor in response to time or an event, with the delay under the kernel's control. LiteRTOS is a small teaching kernel. It is being written from scratch so each mechanism stays visible.

The five steps add one mechanism at a time:

1. Describe a task in memory, without running it.
2. Run two tasks that voluntarily hand the processor to each other.
3. Choose which of several tasks runs, using priority.
4. Let a timer take the processor away from a task.
5. Let a task wait for time or for another task, and share data safely.

## Step 1 — Describe a task

**Status: done.** Tested on the Mac. The processor never enters the task.

A task needs two things before it can run: a record the kernel can hold, and a private stack.

The record is a **task control block** (TCB). It is an ordinary C struct, one per task. It is not a hardware device. In this step the struct stores three facts:

- `sp`, the task's stack pointer. This is the address where its saved registers will sit.
- `stack`, the start of the memory buffer reserved for that task.
- `stack_words`, how many 32-bit words are in the buffer.

The caller provides the buffer. The kernel does not allocate it. Each task gets its own buffer so one task's local variables cannot silently occupy another task's stack.

The **stack** is the region of memory the processor uses for saved registers and for a C function's local variables. On ARM it grows downward: a push stores a word at a lower address. The **stack pointer** is the register that holds the current top of that region.

Step 1 fills the top of the buffer with a fake saved **context**. The context is the set of values required to resume the task: the general-purpose registers, the **program counter** (PC, the address of the next instruction), the **link register** (LR, the return address used if the task function returns), and the **program status register** (xPSR, which holds the condition flags and the bit that says "execute Thumb instructions"). The first argument to the task is stored where the processor will load R0, because the ARM calling convention passes the first argument in R0.

Those words are arranged in the shape a Cortex-M exception would leave behind. The core has two stack pointers. The **Main Stack Pointer** (MSP) is the stack used by reset and by interrupt handlers, which is where kernel code will run. The **Process Stack Pointer** (PSP) is the stack used by tasks. The TCB remembers the PSP. Step 1 only writes memory. It does not load either hardware pointer, so the task does not execute.

After this step you can point a debugger or a unit test at a TCB and see the PC, the argument, and the status register that a future start will load. You cannot observe a task running, and you cannot switch.

## Step 2 — Run two tasks that cooperate

**Status: not started.** This is the first step that uses QEMU, a program on the Mac that behaves like the Cortex-M4 board.

Step 2 takes the memory image from Step 1 and actually loads it. Starting a task means writing its saved stack pointer into the PSP and performing an exception return, which pops the registers and jumps to the task's PC. That return needs a few assembly instructions. C can compute addresses, but it cannot reliably write the special registers that select the stack and trigger the return.

Once two tasks have been started this way, either of them can **yield**. A yield is an explicit call that means "I am willing to pause; run the other task." The kernel saves the current task's registers on its stack, stores the stack pointer in that task's TCB, loads the other TCB's stack pointer, and restores the other task's registers. That save-and-restore is a **context switch**.

This is **cooperative multitasking**. The tasks cooperate. The switch happens at the yield call and nowhere else. If one task enters a long loop and never yields, the other task waits forever, even if it is urgent. Two tasks are enough to see the mechanism: each can print or count, yield, and later resume with its own locals preserved.

Step 2 differs from Step 1 because the processor enters the task code. It differs from later steps because there is no policy and no clock. "The other task" is the only choice, and a task that refuses to yield keeps the core.

## Step 3 — Choose among ready tasks

**Status: not started.** Still cooperative. Still no timer.

Step 2 can only flip between two tasks. Step 3 adds a third task and a **scheduler**, the code that decides who runs next when someone yields.

A task that is allowed to run is **ready**. The kernel keeps track of the ready tasks. Each task also has a **priority**, a small integer the programmer assigns. When a yield happens, the scheduler selects the ready task with the best priority. The context switch itself is the same save-and-restore from Step 2. What changed is the choice of destination.

Priority has a failure mode called **starvation**. If a high-priority task is always ready, a low-priority task never gets chosen, even though it is also ready. Step 3 is where that becomes visible, because there are finally more than two tasks and their priorities can differ. The low-priority task is not broken. The rule "always run the best priority" simply never picks it.

Step 3 still depends on yield. A high-priority task that never yields occupies the core, and the scheduler never gets a chance to pick anyone else. Priority is consulted only at the moments tasks choose to give the processor up.

## Step 4 — Let the clock interrupt a task

**Status: not started.** This is the change from cooperative to preemptive.

**Preemptive multitasking** means the kernel can suspend a task at a moment the task did not choose. The trigger is **SysTick**, a timer built into the Cortex-M core. It counts down and raises an **exception** at a steady rate. An exception is a hardware event that stops the current code, has the processor save a fixed set of registers, and runs a small handler.

The tick handler should stay short. The actual context switch runs in **PendSV**, another Cortex-M exception whose priority is set below ordinary interrupts. The tick handler requests PendSV. PendSV runs when those interrupts are finished, saves the rest of the registers, and switches to the highest-priority ready task. A low-priority task in a long loop can now be suspended without calling yield. When it resumes, it continues at the interrupted instruction.

The saved registers come from two places. On exception entry the hardware automatically stacks R0–R3, R12, LR, PC, and xPSR. That is the same hardware frame Step 1 fabricated by hand. Software, in the PendSV assembly, stacks the remaining callee-saved registers R4–R11. **EXC_RETURN** is a special value the hardware places in LR when the exception begins. Branching to it tells the processor to reload the hardware-saved registers and leave the handler. It is not a normal code address.

Step 4 differs from Step 3 in who gets to decide. Step 3 switches only because a task called yield. Step 4 also switches because time passed. The priority rule is the same. The difference is that the rule is applied on the timer as well as on a yield, so a newly ready high-priority task can take over immediately.

## Step 5 — Wait, signal, and share data

**Status: not started.** Scheduling from Step 4 stays in place. This step adds reasons for a task to leave the ready set, and the objects tasks use to coordinate.

A task is **running** when it currently owns the processor, **ready** when it could run, and **blocked** when it is waiting for something and must not be selected. A delay is the simplest wait: "block me for N ticks, then make me ready." The scheduler skips blocked tasks. A delayed low-priority task no longer needs to spin in a loop, and it no longer needs to yield just to kill time.

Tasks also wait on each other.

A **semaphore** is a counter the kernel maintains, plus a way for a task to wait until the counter is non-zero. A **binary semaphore** only takes the values 0 and 1. A task can wait for it (often called take or pend) and another task or an interrupt can signal it (give or post). It is the usual way to mean "an event happened" or "a slot is free." A **counting semaphore** allows values above 1, so it can mean "N buffers are available" or "N tasks may enter."

A **mutex** (mutual exclusion lock) is a lock with an owner. A task locks it, uses a shared piece of data, and unlocks it. Other tasks that try to lock it block until the owner unlocks. The owner matters. A semaphore does not remember who signaled it. A mutex remembers which task holds it.

Sharing data introduces a **critical section**: a short sequence that must run to completion without another task observing the data halfway through an update. Inside it the kernel briefly refuses to switch. The section stays short because, while it runs, a high-priority task cannot preempt.

**Priority inversion** is what happens when a low-priority task holds a mutex that a high-priority task needs. The high-priority task blocks. A medium-priority task, which does not need the mutex, can then run ahead of the low-priority owner and delay the high-priority task for as long as it likes. Step 5 introduces the situation and a basic way to notice it. A full priority-inheritance protocol can wait until the basic case is clear.

This step also adds **fault handling**, so a bad stack or a bad exception return stops in a known handler instead of wandering off, and **automated tests** that boot the kernel under QEMU and check scheduling, delays, and wakeups.

Step 5 differs from Step 4 because a task can become ineligible without being preempted. In Step 4 every created task is ready, and the timer only swaps among ready tasks. In Step 5 a task can block, disappear from the choice, and become ready again when time expires or another task signals.

## How the steps differ

Each step keeps the previous step's mechanism and adds one new decision.

| | Step 1 | Step 2 | Step 3 | Step 4 | Step 5 |
| --- | --- | --- | --- | --- | --- |
| Question it answers | What is a task made of? | How does the core enter and leave a task? | Which ready task runs? | When can the kernel force a switch? | How does a task wait or share data? |
| Task code runs | No | Yes | Yes | Yes | Yes |
| How many tasks | One or more, prepared only | Two, alternating | Three, chosen by priority | Three or more, same rule | Many; blocked tasks are skipped |
| What causes a switch | Nothing | A call to yield | A call to yield | Yield, or a SysTick interrupt | Yield, a tick, or a wake from a block |
| Who selects the next task | Nobody | The yield switches to the other task | The scheduler, by priority | The scheduler, by priority, also on the tick | The scheduler, by priority, among tasks that are ready |
| A looping task that never yields | It is not running | It keeps the core | It keeps the core | A higher-priority task can preempt it | Same as Step 4; it can also block itself |
| New terms | TCB, stack, context, MSP, PSP | Yield, context switch, cooperative | Scheduler, ready, priority, starvation | Preemptive, SysTick, PendSV, exception, EXC_RETURN | Blocked, semaphore, mutex, critical section, priority inversion |

Reading across a row shows the growth. Step 1 has data and no execution. Steps 2 and 3 execute, and they switch only at yield; Step 3 adds a real choice. Step 4 is the first step in which the task can lose the processor involuntarily. Step 5 is the first step in which "not running" can mean "waiting for an event" rather than "paused until the scheduler picks me."

## Where the work runs

Steps 2–5 run under QEMU on a virtual Cortex-M4 board (`mps2-an386`). Step 1 is a host program because it only checks memory. After the five steps work on the simulator, a later phase builds the same kernel for the STM32F446 Nucleo board. The scheduler and the TCB stay shared. The startup code and the linker script change for the real chip.

## Glossary

| Term | Meaning in this project |
| --- | --- |
| Blocked | A task state: waiting for a delay, a semaphore, or a mutex, and therefore not eligible to run. |
| Concurrent | Several tasks are in progress over the same period of time. On one core their instructions are interleaved. |
| Context | The registers, program counter, status register, and stack pointer needed to resume a task. |
| Context switch | Saving one task's context and restoring another's so the core continues the second task. |
| Cooperative multitasking | A switch happens only when the running task calls yield. |
| Critical section | A short stretch of code during which the kernel will not switch tasks, so a shared update stays consistent. |
| EXC_RETURN | A special link-register value. Branching to it returns from a Cortex-M exception and reloads the registers the hardware saved. |
| Exception | A hardware event that suspends the current code and runs a handler. Interrupts and SysTick are exceptions. |
| Exception frame | The eight words the Cortex-M hardware pushes on exception entry: R0, R1, R2, R3, R12, LR, PC, and xPSR. |
| Kernel | The small body of code that stores task state and decides which task runs. |
| Link register (LR) | The register that holds a return address. In a task's initial context it is where the task goes if its function returns. |
| Main Stack Pointer (MSP) | The stack pointer used by reset and by exception handlers. Kernel code uses this stack. |
| Mutex | A lock with an owner. Other tasks block until the owner unlocks it. Used so only one task at a time touches a piece of shared data. |
| PendSV | A software-requested Cortex-M exception used to perform the context switch after SysTick has asked for one. |
| Preemptive multitasking | The kernel can suspend the running task because a timer fired or a higher-priority task became ready. |
| Priority | A number on the TCB. At a scheduling point the kernel runs the ready task with the best priority. |
| Priority inversion | A low-priority task holds a mutex a high-priority task needs, and a medium-priority task runs ahead of that low-priority owner, delaying the high-priority task. |
| Process Stack Pointer (PSP) | The stack pointer used by tasks. Each task has its own stack, so each TCB stores that task's PSP. |
| Program counter (PC) | The register that holds the address of the instruction the core will execute. |
| Program status register (xPSR) | The register that holds condition flags and the Thumb-state bit. The initial task frame sets the Thumb bit. |
| Ready | A task state: able to run, waiting only for the scheduler to select it. |
| RTOS | Real-time operating system. Here, a kernel that switches tasks under an explicit priority and timing policy. |
| Running | A task state: the task whose instructions the core is executing right now. |
| Scheduler | The kernel code that selects the next task. |
| Semaphore | A kernel counter used for signaling or for limiting how many tasks proceed. Binary: 0 or 1. Counting: zero or more. |
| Stack | Per-task memory for saved registers and C local variables. Grows downward. |
| Starvation | A ready task waits indefinitely because higher-priority tasks are always chosen instead. |
| SysTick | The Cortex-M system timer. It interrupts at a fixed period and is the heartbeat of preemptive scheduling. |
| Task | A function that can pause and resume, with its own stack and its own TCB. On this kernel, tasks share one address space; they are threads, not separate processes. |
| Task control block (TCB) | The struct the kernel keeps for one task: at first the stack pointer and the stack buffer; later, priority, state, and links to other tasks. |
| Thread | Another name for a task that shares its address space with the other tasks. This kernel's tasks are threads. |
| Yield | A call a task makes to give up the processor and let the scheduler run someone else. |
