# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**PandOSsh** (Multi-PandOS) is an educational OS kernel for RISC-V (rv32imafd), designed to run on the **uRISCV** simulator. The project is structured in two phases:
- **Phase 1:** Data structure primitives (PCBs, active semaphore list)
- **Phase 2:** OS nucleus (scheduler, exception handling, interrupts, syscalls)

## Build & Run

**Prerequisites:** `riscv64-unknown-elf-gcc`, `uriscv-elf2uriscv`, `uriscv`

```bash
# Configure and build
cmake -B build
cmake --build build

# Run in simulator
uriscv -c config_machine.json
```

Build output: `build/MultiPandOS.core.uriscv` (loaded by simulator)

There is no automated test runner — tests are compiled into the kernel image via `phase1/p1test.c` or `phase2/p2test.c` and observed through simulator terminal output (`term0.uriscv`).

## Architecture

### Phase 1 — Data Structures (`phase1/`)

- **`pcb.c`**: Manages a static pool of 20 PCBs (`MAXPROC`). Process queues are priority-ordered (`p_prio`: 0=low, 1=high). Process trees use intrusive list linkage (`p_child`, `p_sib`).
- **`asl.c`**: Active Semaphore List. SEMDs are allocated from a free pool on first block and freed when the last process unblocks. Key is the semaphore's memory address (`int *`).
- **`headers/listx.h`**: Linux-style intrusive doubly-linked list macros — all queues are built on this.

### Phase 2 — OS Nucleus (`phase2/`)

**`initial.c`** — Entry point:
1. Sets up the Pass-Up Vector at `0x0FFFF900` with `exceptionHandler` and `uTLB_RefillHandler`
2. Initializes Phase 1 structures, global state, and device semaphore array (`deviceSemaphores[49]`)
3. Creates the first process and calls `scheduler()`

**`scheduler.c`** — Round-robin preemptive dispatch:
- 5ms time slice via PLT (Processor Local Timer)
- Decision tree: dispatch → HALT (0 processes) → WAIT (blocked on I/O) → PANIC (deadlock)
- `start_tod` is snapshotted at each dispatch for CPU time accounting

**`exceptions.c`** — Exception routing via `exceptionHandler()`:
- Routes by cause: SYSCALL (code 8/11) → `syscallHandler()`, TLB (24–28) / program traps → `passUpOrDie()`, interrupts (bit 31) → `interruptHandler()`
- **10 syscalls** (negative numbers): CREATEPROCESS, TERMPROCESS, PASSEREN, VERHOGEN, DOIO, GETTIME, CLOCKWAIT, GETSUPPORTPTR, GETPROCESSID, YIELD
- Non-negative syscall numbers in user mode → privilege trap
- `passUpOrDie()`: passes exception to user-level handler if `p_supportStruct` exists; otherwise kills the entire process subtree recursively

**`interrupts.c`** — Interrupt handling:
- **PLT (bit 7):** time-slice expiry → re-queue current process → scheduler
- **Interval Timer (bit 3):** 100ms pseudo-clock → V() all `CLOCKWAIT` blocked processes
- **Devices (bits 3–7):** read bitmap, find highest-priority device, ACK, V() its semaphore

### Device Semaphore Layout (`deviceSemaphores[49]`)

```
 0– 7: Disk        (IntLine 3)
 8–15: Flash       (IntLine 4)
16–23: Ethernet    (IntLine 5)
24–31: Printer     (IntLine 6)
32–39: Terminal TX (IntLine 7)
40–47: Terminal RX (IntLine 7)
    48: Pseudo-clock (Interval Timer)
```

`softBlockCount` tracks processes blocked on device semaphores (indices 0–47) to distinguish I/O waiting from deadlock.

### Key Constants & Memory Layout

Defined in `headers/const.h`:
- `BIOSDATAPAGE` (`0x0FFFF000`): BIOS data page; exception states saved here by hardware
- `PASSUPVECTOR` (`0x0FFFF900`): Pass-up vector for kernel/TLB handlers
- `KERNELSTACK` (`0x20001000`): Initial kernel stack pointer
- `RAMSTART` (`0x20000000`): Uncached RAM base (KSEG1)
- `UPROCSTARTADDR` (`0x800000B0`): User process entry point
- `USERSTACKTOP` (`0xC0000000`): User stack top
- `MAXPROC` = 20, `SEMDEVLEN` = 49

### PCB Fields to Know

```c
pcb_t.p_s        // Full processor state (registers, PC, status)
pcb_t.p_time     // Accumulated CPU time (microseconds)
pcb_t.p_semAdd   // Non-NULL when blocked on a semaphore
pcb_t.p_supportStruct // NULL for kernel processes; used in Pass-Up
pcb_t.p_prio     // 0 = normal, 1 = high priority
```

### Debug Files
the file ```output.md``` contains the most recent output of the latest run that is been printed in the uriscv terminal. This file is updated frequentry.
the file ```p2testOutput.md``` contans the output that should be displayed in the uriscv terminal after a successful run.

### NOTES
in the ```exceptions.c``` file in the ```static void waitForClock(state_t *savedState)``` function it has been added a ```for``` loop. This loop is essential if the program is run on Linux, using the Linux Mint disto.
