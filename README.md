# HexaOS — Alpha Build

> **Version 4.0 is here!** Fresh features, improvements, and fixes — check the changelog below.

A 32-bit protected-mode hobby OS written in C and x86 assembly, booting from a floppy disk image via QEMU.

## Features

- Custom bootloader (real-mode → protected mode) with floppy disk loading
- VGA text-mode terminal with color support
- Keyboard driver with history scroll
- On-disk persistent storage via ATA PIO (QEMU virtual HDD)
- Multi-user system with login, logout, useradd, passwd, root privileges
- File system (touch, ls, cat, rm, write, append, edit)
- Built-in package manager (`ayo`) — install/remove/update optional feature packages
- Games: Snake, Tic-Tac-Toe, Memory (Simon Says), Hangman, guess the number
- Utilities: calculator, date, cpuinfo, neofetch, lspci, cowsay, cmatrix, fortune, morse code, Russian roulette, insults, excuses, compliments, 8-ball, dice, SL train, BSOD simulation, banner, yes, histogram, palindrome checker, sort, wc, tail, env, echo, color, beep, sleep, random, hex, reverse, string length, palindrome, `kaput`
- Shell commands: help, clear, reboot, halt, panic, shutdown, uptime, about, mem, which, history

## Prerequisites

- `gcc` (i386 / 32-bit support)
- `nasm`
- `ld` (GNU Linker, ELF32 support)
- `objcopy`
- `truncate` (from GNU coreutils)
- `qemu-system-i386`

## Build & Run

```sh
make
make run
```

This produces `os.img` (floppy, 1.44 MB) and `storage.img` (ATA disk, 128 KB), then boots the OS in QEMU.

## Clean

```sh
make clean
```

## Default Users

| User    | Password | Role |
|---------|----------|------|
| `root`  | `root`   | Admin — can use `ayo` to install/remove packages |
| `user`  | `user`   | Standard user |

## What's New in v4.0

- **Dynamic help menu** — `help` now adapts to installed packages
- **Password fixes** — `passwd` now correctly updates the user database
- **`.fun` package gate** — games and fun commands require the `games` package (`ayo add games`)
- **Improved bootloader** — fixed sector count patch and CHS wrap bug
- **HEXA OS logo** — new boot splash
- **Linker map** — build now outputs `link.map` for debugging

## Warnings

- **Alpha quality.** HexaOS is an early-stage hobby OS. Expect bugs, crashes, and missing features. Do **not** rely on it for any real work.
- **QEMU only.** The ATA driver targets QEMU's emulated disk. Running on real hardware is untested and likely will not work.
- **No memory protection.** Paging/MMU is not implemented. Any command (or user code) can corrupt kernel memory, files, or user data.
- **Files are stored on a raw disk image via ATA PIO.** There is no journal, checksum, or redundancy. Power loss or a kernel crash during a write will corrupt the filesystem.
- **Security is minimal.** Passwords are stored in plaintext. There is no encryption or access control beyond a user/root distinction.
- **`ayo` package management** modifies persistent storage. Installing packages as root writes to the disk image; removing critical files can break the system.
- **Games require packages.** Run `ayo add games` as root to enable Snake, Tic-Tac-Toe, Memory, and Hangman.
- **Use at your own risk.** The author provides no guarantees of correctness, safety, or stability.
