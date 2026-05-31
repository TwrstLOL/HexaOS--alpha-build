# HexaOS — Version 5.0 "Paging Edition"

> **v5.0 is here!** Full kernel infrastructure: interrupts, paging, memory management, scheduler, syscalls, and user mode support. This is a major architectural upgrade from v4.0.

A 32-bit protected-mode hobby OS written in C and x86 assembly, booting from a floppy disk image via QEMU.

## What's New in v5.0

### Kernel Foundation
- **Interrupt Descriptor Table (IDT)** — Full 256-entry IDT with exception handlers (0-31), hardware IRQs (32-47), and syscall gate (0x80)
- **PIC Remap** — Master (IRQ0-7 → INT 0x20-0x27) and Slave (IRQ8-15 → INT 0x28-0x2F)
- **PIT Timer** — Channel 0 at 100Hz, drives `system_ticks` counter and scheduler
- **GDT with User Mode** — Ring 0 code/data, Ring 3 code/data, and TSS descriptor for privilege switching

### Memory Management
- **Paging (x86)** — Page directory + page tables, identity maps first 8MB, dynamic page table allocation
- **Physical Memory Manager** — Bitmap-based, manages 0–32MB, tracks each 4KB page
- **Kernel Heap Allocator** — Linked-list `kmalloc`/`kfree` with block splitting and coalescing
- **Page Fault Handler** — Reads CR2, decodes error flags (present/write/user), logs and recovers (system stays alive)

### Exception Handling
- All 32 CPU exceptions have registered handlers
- **Double Fault** (vector 8) halts the system
- **Page Fault** (vector 14) logs fault address + flags, returns
- **GPF** (vector 13) logs EIP + error code, returns
- Other exceptions log message + EIP and return

### Process & Scheduling
- **Task struct** with PID, kernel stack, register save area, state machine
- **Round-Robin Scheduler** — called from timer IRQ, cycles through ready tasks
- **Context Switch** — modifies IRQ stack register save area to switch between tasks
- **`yield()`** — triggers timer IRQ for cooperative rescheduling

### User Boundary
- **Ring 3 code segment** (0x18) and **Ring 3 data segment** (0x20) in GDT
- **Task State Segment (TSS)** with ring 0 stack pointer for syscall/interrupt entry
- **Syscall mechanism** via `int $0x80` (DPL=3 gate, accessible from user code)
- **4 syscalls**: `SYS_PRINT` (print string), `SYS_READ` (read keyboard), `SYS_EXIT` (exit task), `SYS_TICKS` (get tick count)

### Persistence
- ATA PIO block device driver (primary channel, LBA28 addressing)
- Read/write disk sectors for persistent file storage
- Multi-user file system with permissions (owner/group/other, rwx bits)

### Shell & Commands (70+)
```
System:   help, clear, reboot, halt, panic, sleep, shutdown
Hardware: date, cpuinfo, lspci, neofetch, outb, inb
Strings:  echo, len, hex, reverse, tolower, toupper, morse, tr, seq
Apps:     calc, rand, ascii, palette, matrix, guess
Files:    touch, cat, ls, rm, write, append, edit, mv, cp, head, tail
          grep, find, diff, uniq, chmod, chown, pwd, mkdir
Users:    login, logout, useradd, passwd, whoami, id, diese, who
Unix:     alias, unalias, ps, kill, basename, dirname, sort, env, tee
Fun:      banner, fortune, yes, cowsay, cmatrix, logo, sl, morse
          dice, 8ball, russian, insult, excuse, compliment, hack, bsod
Games:    snake, tictactoe, hangman, memory
Info:     uname, uptime, about, mem, beep, history, which
Pkg:      ayo list/add/remove/update
```

### Kernel Subsystems (Architecture)

```
┌─────────────────────────────────────────────────┐
│                   HEXA OS 5.0                    │
├─────────────┬───────────┬───────────────────────┤
│  Interrupts │  Memory   │   Process             │
│  ┌───────┐  │ ┌──────┐  │  ┌────────┐           │
│  │ IDT   │  │ │ PMM  │  │  │ Sched  │           │
│  │ PIC   │  │ │ Paging│  │  │ Tasks  │           │
│  │ PIT   │  │ │ Heap  │  │  │ CtxSw  │           │
│  │ Excp  │  │ │ PFHdl │  │  │ User   │           │
│  └───────┘  │ └──────┘  │  └────────┘           │
├─────────────┴───────────┴───────────────────────┤
│  Drivers:  ATA PIO  ·  Keyboard (IRQ)  ·  VGA   │
│  Syscall:  int 0x80   (print/read/exit/ticks)   │
│  Shell:    HEXA CLI   (70+ commands, aliases)    │
└─────────────────────────────────────────────────┘
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
4. **User Task**: Created with PID=0, enters infinite syscall loop
5. **Userspace**: Loads user database + file system from ATA disk, presents login prompt
6. **Shell**: Interactive CLI with command history, aliases, tab-completion (planned)

## Default Users

| User   | Password | Role  |
|--------|----------|-------|
| `root` | `root`   | Admin — full access, can install packages via `ayo` |
| `user` | `user`   | Standard user — limited permissions |

Create additional users with `useradd <name>` (root only) or type `new` at the login prompt.

## File System

- Flat file system stored on ATA disk image (LBA 100+)
- 64 max files, 512 bytes content per file
- Permission bits: owner/group/other with rwx (e.g., `rw-r--r--`)
- Commands: `touch`, `cat`, `ls`, `rm`, `write`, `append`, `edit`, `mv`, `cp`, `chmod`, `chown`

## Package Manager (`ayo`)

```
ayo list       — list available packages [I]=installed
ayo add <pkg> — install package (root only)
ayo remove    — uninstall package (root only)
ayo update    — refresh installed package files
```

Packages: `games`, `docs`, `fun`, `dev`, `cheatsheet`, `tools`, `net`, `math`, `sysadmin`, `security`, `media`, `productivity`, `science`, `lang`, `puzzle`, `adventure`, `retro`, `algorithms`, `crypto`, `music`, `philosophy`, `geography`

Example: `ayo add games` enables Snake, Tic-Tac-Toe, Hangman, and Memory.

## neofetch Output

```
    __________________________
   /   H E X A   O S   5.0   \
  |  Paging  ·  Scheduler     |
  |  User Mode  ·  Syscalls   |
  |  32-bit Protected Mode    |
   \________________________/
 ┌──────────────────────────────┐
 │  OS:       HEXA OS 5.0 i386  │
 │  Host:     hexaos            │
 │  Version:  5.0 Paging Ed.    │
 │  Kernel:   GenuineIntel      │
 │  Paging:   Enabled (4KB pg)  │
 │  IRQs:     PIC PIT@100Hz     │
 │  Tasks:    1  Scheduler: RR  │
 │  Syscall:  int 0x80          │
 │  User:     Ring3 TSS         │
 └──────────────────────────────┘
```

## Project Structure

```
HexaOS-alpha-build/
├── boot_entry.asm      # Real-mode bootloader (510 bytes + MBR)
├── kernel_entry.asm    # ISR stubs, IRQ stubs, context switch, TSS, user entry
├── hexa.c              # Shell, commands, file system, user management, ATA driver
├── interrupts.c        # IDT setup, PIC remap, PIT init, exception handlers, keyboard IRQ
├── interrupts.h        # IDT/PIC/PIT declarations, regs struct
├── paging.c            # PMM bitmap, page tables, heap allocator
├── paging.h            # Paging/heap declarations
├── scheduler.c         # Task creation, context switch, round-robin scheduler
├── scheduler.h         # Task struct, scheduler declarations
├── syscall.c           # Syscall handler (print/read/exit/ticks)
├── syscall.h           # Syscall numbers
├── types.h             # Type definitions, I/O helpers (outb/inb)
├── link.ld             # Linker script (kernel at 0x10000, _kernel_end)
├── Makefile            # Build system (gcc -m32, nasm, ld, objcopy)
├── link.map            # Linker symbol map (for debugging)
├── REVIEW.md           # Kernel infrastructure review + todo list
└── README.md           # This file
```

## Kernel Memory Map

| Region     | Address        | Description                 |
|------------|----------------|-----------------------------|
| Bootloader | `0x7C00`       | Real-mode boot sector       |
| Stack      | `0x7A00`       | Kernel stack (512 bytes)    |
| Kernel     | `0x10000`      | Kernel code + data + BSS    |
| Heap       | `0x800000`     | Kernel heap (1MB initial)   |
| VGA        | `0xB8000`      | VGA text mode framebuffer   |
| Page Dir   | PMM-allocated  | Page directory (4KB)        |
| Page Table | PMM-allocated  | Page tables (4KB each)      |
| TSS        | Kernel BSS     | Task State Segment (26 dwords) |

## Technical Details

- **CPU Mode**: 32-bit Protected Mode (no long mode)
- **Paging**: 4KB pages, identity-mapped first 8MB, supervisor-only
- **Interrupts**: 8259 PIC, 100Hz PIT timer, PS/2 keyboard on IRQ1
- **Scheduling**: Round-robin, invoked from timer IRQ, saves/restores full register context
- **Syscall**: Software interrupt 0x80, DPL=3 for user-mode access
- **Storage**: ATA PIO (LBA28), primary channel, polling mode
- **Display**: VGA text mode 80×25, 16 colors, hardware cursor

## Warnings

- **Alpha quality.** HexaOS is a hobby OS. Expect bugs, crashes, and missing features.
- **QEMU only.** The ATA driver targets QEMU's emulated disk. Real hardware untested.
- **Minimal security.** Passwords use a simple hash (djb2 variant). No encryption.
- **Games require packages.** Run `ayo add games` as root to enable games.
- **No journaling.** File system writes are direct to disk. Power loss = corruption.
- **Single-core only.** No SMP support.
- **Use at your own risk.** No guarantees of correctness, safety, or stability.
