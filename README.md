# HexaOS — Version 7.0 "Diamond"

> **v7.0 is here!** 8 new kernel subsystems, a transactional filesystem, capability-gated I/O, network stack, boot policy with rollback, HEX binary format, kernel observability, and syscall replay — all under the Diamond codename.

A 32-bit protected-mode hobby OS written in C and x86 assembly, booting from a floppy disk image via QEMU.

## What's New in v7.0

### Version Bump
- **Version** — 6.3 → 7.0 "Diamond"
- ALL banners, help strings, neofetch, login screen, uname, sysinfo, about, logo updated

### 8 New Kernel Subsystems

| Subsystem | Files | What It Does |
|-----------|-------|-------------|
| **HEX Binary Format** | `hex.c`, `hex.h` | Capability-wrapped ELF container (magic `0x48455841`) with input/output schema hashes, dependency hashes, and checksum validation |
| **Boot Policy** | `boot_policy.c`, `boot_policy.h` | Staged boot sequencer (8 stages: hardware, drivers, services, shell) with snapshot tracking and automatic rollback on failure |
| **HEXAFS (VFS Layer)** | `hexafs.c`, `hexafs.h` | Transactional filesystem with abstraction directories, snapshot management, content-addressed object storage, atomic updates |
| **HEXAFS (Disk Driver)** | `hexafs_disk.c`, `hexafs_disk.h` | Block-level layout on 4096-sector (2 MB) disk: superblock, bitmap allocator, CRC-verified object store, snapshot chain with write-ahead journaling |
| **Intent System** | `intent.c`, `intent.h` | Declarative capability-based I/O (CONSUME/PRODUCE/OBSERVE/TRANSFORM) replacing traditional FDs, with compatibility shims for legacy VFS |
| **Kernel Observers** | `kobserve.c`, `kobserve.h` | In-kernel observability via virtual paths (`/@kernel/scheduler/tasks`, `/@kernel/memory/pages`, `/@kernel/memory/heap`, `/@kernel/interrupts/log`, `/@kernel/interrupts/stats`) |
| **Network Stack** | `net.c`, `net.h` | Virtual network interfaces (loopback `lo` at 127.0.0.1), TCP/UDP connection tracking, 4096-byte loopback ring buffer for local IPC |
| **Syscall Replay** | `replay.c`, `replay.h` | Deterministic record-and-replay engine — captures 1024 syscall events with timestamps/PID/args/snapshots, supports dry-run simulation |

### 19 New Shell Commands

```
Diamond:   kstat, netstat, ifconfig, netlog, netrollback, replay, bootlog,
           bootpolicy, setfallback, caps, grantcap, revokecap, hexpack,
           inbox, sendevent, pipes, timels, timediff, timeat
```

### New Syscalls (7 added, now 28 total)
- `SYS_INTENT` (21) — Register a data intent
- `SYS_FULFILL` (22) — Fulfill/provide data for an intent
- `SYS_DIFF` (23) — Object version diff
- `SYS_REPLAY` (24) — Execute a replay
- `SYS_PIPE_TYPED` (25) — Create a typed pipe
- `SYS_EVENT_SEND` (26) — Send an inter-process event
- `SYS_EVENT_POLL` (27) — Poll for events

### Other Changes
- **Password hashing** upgraded to v7.0 format (`"SSSS+IHHHHHHHH"` — 4-hex salt, `+`, 1-hex iteration count, then hash) with backward-compatible migration
- **VFS overhaul** — form/dimensions terminology (type 0 = form, type 1 = dim), permission checking on open, pipe/console I/O typed
- **Boot sequence** extended: kobserve_init → intent_init → replay_init → net_init → hexafs_mount → boot_policy_execute

### Existing Commands (110+ total)
```
clock     Live RTC clock display (updates in real-time)
free      Memory statistics (PMM pages, tasks, files, users)
ping      Real ICMP ping via RTL8139 NIC (e.g. ping 10.0.2.2)
factor    Prime factorization of any number
hexdump   Hex + ASCII dump of file content
du        Disk usage by file
rev       Reverse file content
shasum    DJB2 hash of file
sysinfo   Quick system overview
watch     Repeat a command every ~0.5s
tetris    Full Tetris game with WASD controls
```

### What's Included

### Kernel Foundation
- **Interrupt Descriptor Table (IDT)** — Full 256-entry IDT with exception handlers (0-31), hardware IRQs (32-47), and syscall gate (0x80)
- **PIC Remap** — Master (IRQ0-7 → INT 0x20-0x27) and Slave (IRQ8-15 → INT 0x28-0x2F)
- **PIT Timer** — Channel 0 at 100Hz, drives `system_ticks` counter and scheduler
- **GDT with User Mode** — Ring 0 code/data, Ring 3 code/data, and TSS descriptor for privilege switching

### Memory Management
- **Paging (x86)** — Page directory + page tables, identity maps first 8MB, dynamic page table allocation
- **Physical Memory Manager** — Bitmap-based, manages 0–32MB, tracks each 4KB page
- **Kernel Heap Allocator** — Linked-list `kmalloc`/`kfree` with block splitting and coalescing
- **Page Fault Handler** — Reads CR2, decodes error flags (present/write/user), logs and recovers

### Exception Handling
- All 32 CPU exceptions have registered handlers
- **Double Fault** (vector 8) halts the system
- **Page Fault** (vector 14) logs fault address + flags, returns
- **GPF** (vector 13) logs EIP + error code, returns
- Other exceptions log message + EIP and return

### Process & Scheduling
- **Task struct** with PID, kernel stack, register save area, state machine
- **Round-Robin Scheduler** — called from timer IRQ, cycles through ready tasks
- **Context Switch** — direct stack-switch via `mov %0, %%esp; add $16; popa; iret`
- **`yield()`** — triggers timer IRQ for cooperative rescheduling

### User Boundary
- **Ring 3 code segment** (0x18) and **Ring 3 data segment** (0x20) in GDT
- **Task State Segment (TSS)** with ring 0 stack pointer for syscall/interrupt entry
- **Syscall mechanism** via `int $0x80` (DPL=3 gate, accessible from user code)
- **28 syscalls** defined: print, read, exit, ticks, open, close, write, getpid, sleep, brk, waitpid, kill, pipe, dup, getppid, uname, getcwd, stat, lseek, mmap, munmap, intent, fulfill, diff, replay, pipe_typed, event_send, event_poll

### Persistence
- ATA PIO block device driver (primary channel, LBA28 addressing)
- Read/write disk sectors for persistent file storage
- Multi-user file system with permissions (owner/group/other, rwx bits)
- Transactional HEXAFS layer with write-ahead journaling and snapshot chains

### Shell & Commands (110+)
```
System:   help, clear, reboot, halt, panic, sleep, shutdown
Hardware: date, cpuinfo, lspci, neofetch, outb, inb, sysinfo
Strings:  echo, len, hex, reverse, tolower, toupper, morse, tr, seq
Apps:     calc, rand, ascii, palette, matrix, guess, ping, factor
Files:    touch, cat, ls, rm, write, append, edit, mv, cp, head, tail,
          grep, find, diff, uniq, chmod, chown, pwd, mkdir, hexdump,
          du, rev, shasum
Users:    login, logout, useradd, passwd, whoami, id, diese, who
Unix:     alias, unalias, ps, kill, basename, dirname, sort, env, tee,
          watch, clock, free
Diamond:  kstat, netstat, ifconfig, netlog, netrollback, replay, bootlog,
          bootpolicy, setfallback, caps, grantcap, revokecap, hexpack,
          inbox, sendevent, pipes, timels, timediff, timeat
Fun:      banner, fortune, yes, cowsay, cmatrix, logo, sl, morse,
          dice, 8ball, russian, insult, excuse, compliment, hack, bsod
Games:    snake, tictactoe, hangman, memory, tetris
Info:     uname, uptime, about, mem, beep, history, which, dmesg
Pkg:      ayo list/add/remove/update
```

### Kernel Subsystems (Architecture)

```
┌──────────────────────────────────────────────────────────┐
│                    HEXA OS 7.0 Diamond                    │
├─────────────┬───────────┬───────────┬────────────────────┤
│  Interrupts │  Memory   │  Process  │  New Subsystems    │
│  ┌───────┐  │ ┌──────┐  │ ┌────────┐│ ┌──────────┐      │
│  │ IDT   │  │ │ PMM  │  │ │ Sched  ││ │ HEX      │      │
│  │ PIC   │  │ │ Paging│  │ │ Tasks  ││ │ Intent   │      │
│  │ PIT   │  │ │ Heap  │  │ │ CtxSw  ││ │ Kobserve │      │
│  │ Excp  │  │ │ PFHdl │  │ │ User   ││ │ BootPol  │      │
│  └───────┘  │ └──────┘  │ └────────┘│ │ HexaFS   │      │
│             │           │           │ │ Net      │      │
│             │           │           │ │ Replay   │      │
│             │           │           │ └──────────┘      │
├─────────────┴───────────┴───────────┴────────────────────┤
│  VFS:  Forms/Dims · Pipes · Console · Permission gating  │
│  Syscall:  int 0x80  (28 syscalls, intent-based I/O)     │
│  Shell:  HEXA CLI v7.0  (110+ commands, Diamond cmds)    │
│  Games:  Snake, TTT, Hangman, Memory, Tetris             │
└──────────────────────────────────────────────────────────┘
```

## Prerequisites

- `gcc` (i386 / 32-bit support, `-m32` flag)
- `nasm` (Netwide Assembler)
- `ld` (GNU Linker, ELF32 support)
- `objcopy` (GNU Binutils)
- `truncate` (GNU coreutils)
- `qemu-system-i386`

## Build & Run

```sh
make clean
make
make run
```

This produces:
- `os.img` — floppy disk image (1.44 MB) containing bootloader + kernel
- `storage.img` — ATA hard disk image (2 MB) for persistent file storage (HEXAFS)
- `kernel.elf` — ELF binary for debugging (symbols in `link.map`)

Boots in QEMU with:
```
qemu-system-i386 -fda os.img -hda storage.img -boot order=a -nographic
```

## Boot Sequence

1. **Real Mode Bootloader** (`boot_entry.asm`): Enables A20 gate, loads kernel from floppy using CHS addressing with 64KB bank switching, loads GDT, enters protected mode, jumps to kernel at 0x10000
2. **Kernel Entry** (`hexa.c` kernel_main): Clears BSS, seeds RNG from CMOS, initializes all subsystems
3. **Subsystem Init**: GDT → IDT → PIC → PIT → PMM → Paging → Heap → Log → **kobserve** → **intent** → **replay** → **net** → Scheduler → Enable Interrupts
4. **Storage Init**: VFS → ATA → **hexafs_mount** → **boot_policy_execute**
5. **User Task**: Created with PID=1, enters infinite syscall loop (`_user_entry`)
6. **Shell Task**: `kernel_main` runs the shell/CLI loop (round-robin with user task)
7. **Round-Robin**: 2 tasks (user + shell) cycle at 100Hz via timer IRQ
8. **Userspace**: Loads user database + file system from ATA disk, presents login prompt

## Default Users

| User   | Password | Role  |
|--------|----------|-------|
| `root` | `root`   | Admin — full access, can install packages via `ayo` |

Create additional users with `useradd <name>` (root only) or type `new` at the login prompt.

## File System

- HEXAFS transactional filesystem on ATA disk image (4096 sectors, 2 MB)
- Superblock with magic, bitmap allocator, CRC-verified object store
- Snapshot chains with linked-list structure for versioned rollback
- Abstraction directories (named containers)
- Content-addressed object storage with DJB2 hashing
- Write-ahead journal for crash-safe metadata updates
- 64 max files, dynamic content allocation via kmalloc
- Permission bits: owner/group/other with rwx (e.g., `rw-r--r--`)
- Commands: `touch`, `cat`, `ls`, `rm`, `write`, `append`, `edit`, `mv`, `cp`, `chmod`, `chown`, `hexdump`, `du`, `rev`, `shasum`

## Package Manager (`ayo`)

```
ayo list       — list available packages [I]=installed
ayo add <pkg> — install package (root only)
ayo remove    — uninstall package (root only)
ayo update    — refresh installed package files
```

Packages: `games`, `docs`, `fun`, `dev`, `cheatsheet`, `tools`, `net`, `math`, `sysadmin`, `security`, `media`, `productivity`, `science`, `lang`, `puzzle`, `adventure`, `retro`, `algorithms`, `crypto`, `music`, `philosophy`, `geography`

Example: `ayo add games` enables Snake, Tic-Tac-Toe, Hangman, Memory, and Tetris.

## neofetch Output

```
    __________________________
   /   H E X A   O S   7.0   \
  |  Diamond · HexaFS · Net  |
  |  110+ Cmds  · 28 Syscalls |
  |  32-bit Protected Mode    |
   \________________________/
 ┌────────────────────────────────┐
 │  OS:       HEXA OS 7.0 i386    │
 │  Host:     hexaos              │
 │  Version:  7.0 "Diamond"       │
 │  Kernel:   GenuineIntel        │
 │  Paging:   Enabled (4KB pg)    │
 │  IRQs:     PIC PIT@100Hz       │
 │  Tasks:    2  Scheduler: RR    │
 │  Syscall:  int 0x80            │
 │  User:     Ring3 TSS           │
 └────────────────────────────────┘
```

## Project Structure

```
HexaOS-alpha-build/
├── boot_entry.asm      # Real-mode bootloader (510 bytes + MBR)
├── kernel_entry.asm    # ISR stubs, IRQ stubs, context switch, TSS, user entry
├── hexa.c              # Shell, 110+ commands, file system, user management, ATA driver, Tetris
├── interrupts.c        # IDT setup, PIC remap, PIT init, exception handlers, keyboard IRQ
├── interrupts.h        # IDT/PIC/PIT declarations, regs struct
├── paging.c            # PMM bitmap, page tables, heap allocator
├── paging.h            # Paging/heap declarations
├── process.c           # Task creation, context switch, round-robin scheduler
├── process.h           # Task struct, scheduler declarations
├── syscall.c           # Syscall handler (28 syscalls)
├── syscall.h           # Syscall numbers
├── vfs.c               # Virtual File System layer (forms/dims)
├── vfs.h               # VFS declarations
├── pipe.c              # Inter-process pipes
├── pipe.h              # Pipe declarations
├── sync.c              # Mutex and semaphore
├── sync.h              # Sync declarations
├── log.c               # Kernel ring-buffer logging
├── log.h               # Log declarations
├── driver.c            # Block/char device abstraction
├── driver.h            # Driver declarations
├── elf.c               # ELF binary loader
├── elf.h               # ELF header structures
├── boot_policy.c       # Staged boot sequencer with rollback
├── boot_policy.h       # Boot policy declarations
├── hex.c               # HEX binary container format
├── hex.h               # HEX format declarations
├── hexafs.c            # HEXAFS transactional filesystem (VFS layer)
├── hexafs.h            # HEXAFS VFS declarations
├── hexafs_disk.c       # HEXAFS block-level disk driver
├── hexafs_disk.h       # HEXAFS disk declarations
├── intent.c            # Declarative capability-based I/O
├── intent.h            # Intent system declarations
├── kobserve.c          # In-kernel observability framework
├── kobserve.h          # Kobserve declarations
├── net.c               # Network stack (virtual interfaces, loopback)
├── net.h               # Net declarations
├── replay.c            # Syscall record-and-replay engine
├── replay.h            # Replay declarations
├── types.h             # Type definitions, I/O helpers
├── link.ld             # Linker script (kernel at 0x10000)
├── Makefile            # Build system (gcc -m32, nasm, ld, objcopy)
├── REVIEW.md           # Kernel infrastructure review
└── README.md           # This file
```

## Kernel Memory Map

| Region     | Address        | Description                 |
|------------|----------------|-----------------------------|
| Bootloader | `0x7C00`       | Real-mode boot sector       |
| Stack      | `0x7A00`       | Kernel stack (512 bytes)    |
| Kernel     | `0x10000`      | Kernel code + data + BSS (~500KB) |
| Heap       | `0x800000`     | Kernel heap (1MB initial)   |
| VGA        | `0xB8000`      | VGA text mode framebuffer   |
| Page Dir   | PMM-allocated  | Page directory (4KB)        |
| Page Table | PMM-allocated  | Page tables (4KB each)      |
| TSS        | Kernel BSS     | Task State Segment          |

## Technical Details

- **CPU Mode**: 32-bit Protected Mode (no long mode)
- **Paging**: 4KB pages, identity-mapped first 8MB, supervisor-only
- **Interrupts**: 8259 PIC, 100Hz PIT timer, PS/2 keyboard on IRQ1
- **Scheduling**: Round-robin, invoked from timer IRQ, saves/restores full register context
- **Syscall**: Software interrupt 0x80, DPL=3 for user-mode access, 28 syscalls
- **VFS**: Form/dim terminology, file descriptors, pipes, console I/O, stat, permission gating
- **Storage**: ATA PIO (LBA28), primary channel, polling mode, HEXAFS transactional layer
- **Intent I/O**: Capability-gated declarative I/O replacing raw FDs
- **Observability**: Virtual `/@kernel/...` paths for runtime kernel introspection
- **Network**: Virtual interfaces with loopback, TCP/UDP tracking
- **Display**: VGA text mode 80×25, 16 colors, hardware cursor

## Warnings

- **Alpha quality.** HexaOS is a hobby OS. Expect bugs, crashes, and missing features.
- **QEMU only.** The ATA driver targets QEMU's emulated disk. Real hardware untested.
- **Minimal security.** Passwords use a simple hash (djb2 variant). No encryption.
- **Games require packages.** Run `ayo add games` as root to enable games.
- **No journaling.** File system writes are direct to disk. Power loss = corruption.
- **Single-core only.** No SMP support.
- **Use at your own risk.** No guarantees of correctness, safety, or stability.
