# HexaOS — Version 6.0 "VFS Edition"

> **v6.0 is here!** Major upgrade with VFS bugfixes, expanded file storage, 20+ new commands, Tetris game, keyboard improvements, and new syscalls. The biggest feature release yet!

A 32-bit protected-mode hobby OS written in C and x86 assembly, booting from a floppy disk image via QEMU.

## What's New in v6.0

### Critical Bugfixes
- **VFS file read/write FIXED** — Previously `vfs_read` returned spaces and `vfs_write` was a no-op. Now both read/write actual file content through the proper file table. This is the biggest fix in v6.0.
- **VFS `stat()` implemented** — The `stat` syscall now returns real file metadata instead of -1.
- **Keyboard IRQ arrow keys fixed** — Arrow keys from the PS/2 IRQ keyboard now properly trigger history navigation in the shell.

### Expanded Storage
- **File content enlarged 4x** — From 512 bytes to 2006 bytes per file (using 4 ATA sectors instead of 2).
- **SYS_STAT and SYS_GETCWD syscalls** — New syscalls wired up for user program support.

### New Commands (20+ added, 90+ total)
```
clock     Live RTC clock display (updates in real-time)
free      Memory statistics (PMM pages, tasks, files, users)
ping      Simulated ping with 4-packet stats
factor    Prime factorization of any number
hexdump   Hex + ASCII dump of file content
du        Disk usage by file
rev       Reverse file content
shasum    DJB2 hash of file
sysinfo   Quick system overview
watch     Repeat a command every ~0.5s
tetris    Full Tetris game with WASD controls
```

### New Game: Tetris!
- Full 10x20 Tetris playfield
- All 7 tetrominoes (I, O, T, S, Z, J, L) with rotation
- Gravity, line clearing, level progression
- Score tracking with level multiplier
- WASD controls (W=rotate, A=left, D=right, S=drop)

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
- **21 syscalls** defined: print, read, exit, ticks, open, close, write, getpid, sleep, brk, waitpid, kill, pipe, dup, getppid, uname, getcwd, stat, lseek, mmap, munmap

### Persistence
- ATA PIO block device driver (primary channel, LBA28 addressing)
- Read/write disk sectors for persistent file storage
- Multi-user file system with permissions (owner/group/other, rwx bits)
- File content expanded to 2006 bytes per file

### Shell & Commands (90+)
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
Fun:      banner, fortune, yes, cowsay, cmatrix, logo, sl, morse,
          dice, 8ball, russian, insult, excuse, compliment, hack, bsod
Games:    snake, tictactoe, hangman, memory, tetris
Info:     uname, uptime, about, mem, beep, history, which, dmesg
Pkg:      ayo list/add/remove/update
```

### Kernel Subsystems (Architecture)

```
┌──────────────────────────────────────────────────┐
│                   HEXA OS 6.0                    │
├─────────────┬───────────┬────────────────────────┤
│  Interrupts │  Memory   │   Process              │
│  ┌───────┐  │ ┌──────┐  │  ┌────────┐            │
│  │ IDT   │  │ │ PMM  │  │  │ Sched  │            │
│  │ PIC   │  │ │ Paging│  │  │ Tasks  │            │
│  │ PIT   │  │ │ Heap  │  │  │ CtxSw  │            │
│  │ Excp  │  │ │ PFHdl │  │  │ User   │            │
│  └───────┘  │ └──────┘  │  └────────┘            │
├─────────────┴───────────┴────────────────────────┤
│  VFS:  Read/Write/Stat  ·  Pipes  ·  Console     │
│  Syscall:  int 0x80  (21 syscalls, 6 active)     │
│  Shell:  HEXA CLI v6.0  (90+ commands, aliases)  │
│  Games:  Snake, TTT, Hangman, Memory, Tetris     │
└──────────────────────────────────────────────────┘
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
- `storage.img` — ATA hard disk image (128 KB) for persistent file storage
- `kernel.elf` — ELF binary for debugging (symbols in `link.map`)

Boots in QEMU with:
```
qemu-system-i386 -fda os.img -hda storage.img -boot order=a -nographic
```

## Boot Sequence

1. **Real Mode Bootloader** (`boot_entry.asm`): Enables A20 gate, loads kernel from floppy using CHS addressing with 64KB bank switching, loads GDT, enters protected mode, jumps to kernel at 0x10000
2. **Kernel Entry** (`hexa.c` kernel_main): Clears BSS, seeds RNG from CMOS, initializes all subsystems
3. **Subsystem Init**: GDT → IDT → PIC → PIT → PMM → Paging → Heap → Scheduler → Enable Interrupts
4. **User Task**: Created with PID=1, enters infinite syscall loop (`_user_entry`)
5. **Shell Task**: `kernel_main` runs the shell/CLI loop (round-robin with user task)
6. **Round-Robin**: 2 tasks (user + shell) cycle at 100Hz via timer IRQ
7. **Userspace**: Loads user database + file system from ATA disk, presents login prompt

## Default Users

| User   | Password | Role  |
|--------|----------|-------|
| `root` | `root`   | Admin — full access, can install packages via `ayo` |

Create additional users with `useradd <name>` (root only) or type `new` at the login prompt.

## File System

- Flat file system stored on ATA disk image (LBA 100+)
- 64 max files, 2006 bytes content per file (up from 512 in v5.1)
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
   /   H E X A   O S   6.0   \
  |  VFS  ·  Tetris  ·  Pipe  |
  |  90+ Cmds  ·  ATA Storage |
  |  32-bit Protected Mode    |
   \________________________/
 ┌──────────────────────────────┐
 │  OS:       HEXA OS 6.0 i386  │
 │  Host:     hexaos            │
 │  Version:  6.0 "VFS Edition" │
 │  Kernel:   GenuineIntel      │
 │  Paging:   Enabled (4KB pg)  │
 │  IRQs:     PIC PIT@100Hz     │
 │  Tasks:    2  Scheduler: RR  │
 │  Syscall:  int 0x80          │
 │  User:     Ring3 TSS         │
 └──────────────────────────────┘
```

## Project Structure

```
HexaOS-alpha-build/
├── boot_entry.asm      # Real-mode bootloader (510 bytes + MBR)
├── kernel_entry.asm    # ISR stubs, IRQ stubs, context switch, TSS, user entry
├── hexa.c              # Shell, 90+ commands, file system, user management, ATA driver, Tetris
├── interrupts.c        # IDT setup, PIC remap, PIT init, exception handlers, keyboard IRQ
├── interrupts.h        # IDT/PIC/PIT declarations, regs struct
├── paging.c            # PMM bitmap, page tables, heap allocator
├── paging.h            # Paging/heap declarations
├── process.c           # Task creation, context switch, round-robin scheduler
├── process.h           # Task struct, scheduler declarations
├── syscall.c           # Syscall handler (21 syscalls)
├── syscall.h           # Syscall numbers
├── vfs.c               # Virtual File System layer
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
- **Syscall**: Software interrupt 0x80, DPL=3 for user-mode access, 21 syscalls
- **VFS**: File descriptors, pipes, console I/O, stat support
- **Storage**: ATA PIO (LBA28), primary channel, polling mode, 2006B per file
- **Display**: VGA text mode 80×25, 16 colors, hardware cursor

## Warnings

- **Alpha quality.** HexaOS is a hobby OS. Expect bugs, crashes, and missing features.
- **QEMU only.** The ATA driver targets QEMU's emulated disk. Real hardware untested.
- **Minimal security.** Passwords use a simple hash (djb2 variant). No encryption.
- **Games require packages.** Run `ayo add games` as root to enable games.
- **No journaling.** File system writes are direct to disk. Power loss = corruption.
- **Single-core only.** No SMP support.
- **Use at your own risk.** No guarantees of correctness, safety, or stability.
