# RISC-V Proxy Operating System Kernel

A multi-core, concurrency-safe proxy operating system kernel for the 64-bit RISC-V architecture, developed on the [PKE (Proxy Kernel for Education)](https://gitee.com/hustos/pke-doc) framework and running on the [Spike](https://github.com/riscv/riscv-isa-sim) simulator. The system boots on a configurable number of CPU harts, schedules user processes across them, and provides a Unix-like environment complete with Sv39 virtual memory, copy-on-write fork, a virtual file system switch, kernel synchronization primitives, and a custom zsh-style shell.

The project starts where the PKE lab series ends: instead of a "just-enough" proxy kernel for a single application, this kernel runs a full interactive shell with pipelines, aliases, and persistent history — concurrently, on multiple cores.

## Key Features

### Symmetric Multiprocessing (SMP)
- Supports a **configurable number of harts** — set `NCPU` in `kernel/config.h` and pass the matching `spike -p<N>` — booted through machine-mode startup (`kernel/machine/`), with an atomic counter-based barrier (`kernel/sync_utils.h`) synchronizing hart bring-up before entering supervisor mode.
- **Per-hart run queues** protected by per-hart spin locks, eliminating the global scheduler bottleneck; round-robin time-slicing driven by per-hart timer interrupts.
- **Cross-hart process dispatch**: the `fork_to(hartid)` syscall places a child process directly onto a target hart's ready queue, and the shell exposes this as an affinity-style launch mechanism.
- Graceful SMP shutdown: a global shutdown flag plus a final barrier lets all harts exit cleanly.

### Memory Management
- **Sv39** three-level page tables with per-process address spaces; memory safety enforced through page-table permission bits (`PTE_R/W/X/U`).
- Physical page allocator backed by a free list, with **per-page reference counting** that underpins copy-on-write.
- **Copy-on-Write (COW) fork**: parent and child share physical pages via a `PTE_COW` permission bit; a write fault on a shared page triggers the fault handler in `strap.c` to allocate a private copy — minimizing `fork` memory overhead.
- User-level heap manager (`better_malloc`/`better_free`) over a growable heap segment; the kernel allocation strategy is compile-time selectable (first-fit by default; best-fit / next-fit available via `MEM_ALLOC_STRATEGY` in `kernel/config.h`).

### Processes & Synchronization
- `fork`, `exec`, `wait`, `exit`, `yield` with zombie reaping; **Round-Robin scheduling** on every hart with timer-interrupt preemption; stress-tested to dozens of concurrent processes (`test/bench_src/app_proc_stress.c`).
- **Kernel counting semaphores** (`sem_new` / `sem_P` / `sem_V`) with blocking wait queues, validated by a producer–consumer user program.
- Spin locks and barrier primitives (`kernel/sync_utils.h`, `kernel/lock.h`) built on RISC-V atomics (`amoswap`).

### Virtual File System
- A **VFS layer** (`kernel/vfs.c`) with a unified dentry/inode/superblock abstraction and multiple mountable backends:
  - **RFS**: a RAM-disk file system with directory cache.
  - **hostfs**: pass-through to the host filesystem over Spike's HTIF interface.
- Full path resolution supporting **relative paths** and per-process current working directory; hard links (`link`/`unlink`), `mkdir`, `opendir`/`readdir`, `lseek`, `stat`.
- **Anonymous pipes** (`pipline_write`/`pipline_read`) enabling shell pipelines between processes.

### zshell — a zsh-style user shell
A custom shell (`zshell/`) with its own hand-written tokenizer/parser:
- Built-in commands: `cd`, `pwd`, `ls`, `cat`, `echo`, `mkdir`, `touch`, `exec`, and external program launching.
- **Pipelines** over kernel-supported anonymous pipes (IPC), **aliases**, **environment-variable** expansion, and a `.zshrc` startup file parser.
- **Persistent command history** stored in `/.zsh_history` on the RFS volume.
- **Dynamic multi-core task placement**: background programs can be launched onto a chosen hart via the `fork_to` syscall, exercising the kernel's cross-hart scheduling path.

### Debugging & Observability
- **Stack backtrace** from user space (`print_backtrace` syscall) for crash diagnosis.
- **Error-line diagnosis**: on an illegal instruction, the kernel walks DWARF `.debug_line` data to report the exact offending source line of the user program.
- Source-level kernel debugging with GDB over OpenOCD (`make gdb`).

### Benchmark & Test Infrastructure
- An automated one-command suite (`test/run_all_metrics.sh`) covering functional correctness (COW, semaphore, relative path, backtrace, exec/wait), stress tests, and throughput/latency benchmarks.
- LMbench-style measurements (`lat_proc_fork`, `lat_proc_exec`), fork/alloc throughput, file-I/O bandwidth, and pipe ping-pong latency; results aggregated into TSV summaries and a ready-to-paste Markdown report under `test/results/`.

## Architecture

```
        ┌──────────────────────────── User space ────────────────────────────┐
        │  zshell (parser / aliases / history / pipelines)                   │
        │  user apps: ls cat echo mkdir touch exec cow semaphore bench ...   │
        │  user_lib: printu scanfu better_malloc naive_malloc syscalls (ecall)│
        └──────────────────────────────────┬─────────────────────────────────┘
                                           │ 38 syscalls (ecall / SBI)
        ┌──────────────────────────────────┴─────────────────────────────────┐
Kernel  │  trap layer (strap.c)  — syscall / exception / timer-IRQ dispatch   │
(S-mode)│  scheduler (sched.c)   — per-hart RR run queues, cross-hart fork_to │
        │  process (process.c)   — fork/exec/wait/exit, ELF loader            │
        │  memory (vmm.c/pmm.c)  — SV39 page tables, COW, ref-counted pages   │
        │  sync (semaphore.c, lock.h, sync_utils.h) — semaphores, spinlocks   │
        │  VFS (vfs.c) ─┬─ RFS (rfs.c, RAM disk)                              │
        │               └─ hostfs (hostfs.c, HTIF pass-through)               │
        └──────────────────────────────────┬─────────────────────────────────┘
Machine │  boot & trap vectors (mentry.S, minit.c, mtrap.c), per-hart timer   │
(M-mode)│  Spike interface (HTIF console / host file syscalls)                │
        └─────────────────────────────────────────────────────────────────────┘
```

## Repository Layout

    .
    ├── kernel/            # the OS kernel (trap, sched, process, mm, vfs, sync)
    │   ├── machine/       # machine-mode boot, trap vectors, per-hart timer init
    │   ├── sched.c        # per-hart round-robin scheduler, cross-hart dispatch
    │   ├── process.c      # PCB pool, fork/exec/wait, heap manager
    │   ├── vmm.c / pmm.c  # SV39 paging, COW fault handling, physical page allocator
    │   ├── vfs.c          # virtual file system switch
    │   ├── rfs.c          # RAM-disk file system
    │   ├── hostfs.c       # host pass-through file system (Spike HTIF)
    │   └── semaphore.c    # kernel counting semaphores
    ├── user/              # user-space programs (app_*.c) and the user library
    ├── zshell/            # zsh-style shell: tokenizer, parser, executor, .zshrc, history
    ├── spike_interface/   # HTIF / device-tree glue for the Spike emulator
    ├── util/              # string, printf, atomics helpers
    ├── test/              # automated functional + performance benchmark suite
    └── Makefile

## Getting Started

### Prerequisites
- 64-bit Linux environment (native, VM, WSL, or the provided Docker image)
- RISC-V cross toolchain (`riscv64-unknown-elf-gcc`, ...)
- The Spike ISA simulator (`spike`)

### Option A — Docker (recommended, reproducible)

```bash
$ ./docker_mount.sh        # creates/enters a container with toolchain + Spike preinstalled
```

(`docker_install.sh` shows how the image's toolchain was built, for reference.)

### Option B — build the environment manually

```bash
# 1. build dependencies
$ sudo apt-get install autoconf automake autotools-dev curl python3 libmpc-dev \
    libmpfr-dev libgmp-dev gawk build-essential bison flex device-tree-compiler

# 2. RISC-V cross-compiler
$ export RISCV=/path-to-install-RISCV-toolchains
$ git clone --recursive https://github.com/riscv/riscv-gnu-toolchain.git
$ cd riscv-gnu-toolchain && ./configure --prefix=$RISCV
$ make -j$(nproc) && sudo make install && cd ..

# 3. Spike emulator
$ git clone https://github.com/riscv/riscv-isa-sim.git
$ cd riscv-isa-sim && mkdir build && cd build
$ ../configure --prefix=$RISCV
$ make -j$(nproc) && sudo make install
```

### Build and run

```bash
$ make            # build the kernel and all user programs
$ ./run.sh        # rebuild, then boot the machine into zshell
```

`run.sh` launches `spike -p<N> obj/riscv-pke /bin/zsh` (N = `NCPU` in `kernel/config.h`, 4 by default), dropping you into the interactive shell on a multi-core virtual machine:

```
$ pwd
/
$ ls
$ echo hello > greeting.txt && cat greeting.txt
$ mkdir docs && cd docs
$ exec /bin/app_cow        # run a demo program
```

`make run` instead boots a scripted sequence of demo applications (shell, exec, COW, semaphore, multicore, page-fault, backtrace, error-line, ...).

### Benchmarks

```bash
$ bash test/run_all_metrics.sh            # build + run the full suite
$ bash test/run_all_metrics.sh --skip-build
```

Each run writes `detail.tsv`, `summary.tsv`, `derived_metrics.txt`, and `report.md` into a timestamped directory under `test/results/`, covering fork/allocation throughput, file-I/O bandwidth, pipe round-trip latency, LMbench-style process-latency numbers, and multi-process scaling.

### Debugging (optional)

With [riscv-openocd](https://github.com/riscv/riscv-openocd) installed:

```bash
$ make gdb        # spike (remote bitbang) + openocd + riscv64-unknown-elf-gdb
```

## Acknowledgements

This kernel is built upon **PKE (Proxy Kernel for Education)**, an open-source teaching operating system from Huazhong University of Science and Technology (see [LICENSE.txt](./LICENSE.txt)). The SMP scheduler, copy-on-write memory subsystem, VFS with dual file-system backends, kernel semaphores, zshell, and the benchmark infrastructure were designed and implemented on top of that base.
