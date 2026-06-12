# HexaOS — Kernel Infrastructure Review

## Status: v6.3 "VFS Edition" — Block FS, NIC driver, real ping, exec, password hardening

### ✅ Boot cleanly
- Stable bootloader (real mode → protected mode, floppy CHS load with bank switching)
- GDT set up with ring 0 code/data, ring 3 code/data, and TSS descriptor
- IDT set up with 256 entries (all exceptions + IRQ 0-15 + syscall INT 0x80)
- VGA text console + serial output working

### ✅ Interrupts and time
- Interrupts enabled (PIC remapped to INT 0x20-0x2F)
- PIT timer IRQ (IRQ0 → INT 0x20) at 100Hz, `system_ticks` counter
- Keyboard IRQ (IRQ1 → INT 0x21) with ring buffer, scancode-to-ASCII mapping
- **v6.0**: Arrow keys from IRQ keyboard now properly mapped for history navigation
- **v6.1**: Clock command rewritten with direct VGA/serial output, Tetris rebuilt with uint16 bitmask shape data
- **v6.2**: `panic` command overhauled with 5-stage simulated crash, root required
- **v6.3**: Password hashing hardened (16-bit salt, 5 iters, lazy migration), write-ahead journal for atomic saves, block-based filesystem with dynamic kmalloc content, RTL8139 NIC driver + ICMP ping, exec command (ELF loader wired up)

### ✅ Memory baseline
- Paging enabled (identity map first 8MB + dynamic page table allocation)
- Physical Memory Manager (bitmap-based, manages up to 32MB)
- Kernel heap allocator (linked-list, first-fit with splitting/merging)
- Page fault handler logs fault info and returns (system stays alive)

### ✅ Kernel structure
- Separate files: `kernel_entry.asm`, `interrupts.c/h`, `paging.c/h`, `process.c/h`, `syscall.c/h`, `vfs.c/h`, `pipe.c/h`, `sync.c/h`, `log.c/h`, `driver.c/h`, `elf.c/h`, `hexa.c`
- Clear separation: interrupts, memory, scheduler, syscalls, VFS, drivers, shell
- Shared type definitions in `types.h`, I/O helpers in `types.h`

### ✅ VFS (Fixed in v6.0)
- **File read/write FIXED** — Previously returned spaces (read) or no-oped (write). Now properly reads/writes via f_table.
- **vfs_stat() implemented** — Returns real file metadata (name, size, owner, mode, type)
- FD management for files, pipes, console (stdin/stdout/stderr)
- 128 global FD slots

### ✅ Process & Scheduler
- Task struct with kernel stack, state, PID, page directory, FD table
- Task creation (`proc_create`) with saved register context
- Round-robin scheduler via timer IRQ
- 2-task scheduling (user task + shell) verified stable
- Context switch by direct stack manipulation

### ✅ User boundary
- Ring 3 code segment (0x18) and data segment (0x20) in GDT
- TSS with ring 0 stack for syscall/interrupt handling
- Syscall mechanism via INT 0x80 (DPL=3 gate)
- **v6.0**: 21 syscalls defined (SYS_PRINT through SYS_MUNMAP), 6 implemented
- **v6.1**: Clock and Tetris commands fixed and working
- **v6.2**: `panic` command rewritten: register dump, stack trace, memory scan, filesystem check, recovery prompt, countdown shutdown
- **v6.3**: `exec` command loads ELF binaries from filesystem into user tasks, RTL8139 NIC driver + real ICMP ping

### ✅ Persistence
- ATA PIO block device driver (primary channel, LBA28)
- Read/write sectors (proven working — file system loads/stores)
- Storage persists via `storage.img` (QEMU virtual HDD)
- **v6.0**: File content expanded from 512 to 2006 bytes (4 ATA sectors per file)
- **v6.1**: Version bumped to 6.1, all docs and banners updated
- **v6.2**: Version bumped to 6.2, panic command rewritten
- **v6.3**: Version bumped to 6.3, block-based FS (dynamic kmalloc content + block-chain storage), write-ahead journal, storage image 4096 sectors

### ✅ Control loop
- Page faults are logged and recovered (system continues)
- GPF is logged and recovered
- Other exceptions are logged and recovered
- Double fault halts (unrecoverable by definition)

### ✅ Shell & Commands (90+)
- **v6.0**: Added 12+ new commands: `clock`, `free`, `ping`, `factor`, `hexdump`, `du`, `rev`, `shasum`, `sysinfo`, `watch`, `tetris`
- **v6.1**: `clock` rewritten (direct VGA+serial), `tetris` rebuilt with correct bitmask shape data
- **v6.2**: `panic` overhauled — 5 stages, register dump, stack trace, memory map, filesystem scan, recovery/countdown
- Fixed IRQ keyboard arrow key support for history navigation
- **v6.3**: `exec` command — loads ELF binary from filesystem, creates user task; `ping` rebuilt as real RTL8139 ICMP echo

## Recent Changes (v6.3)

| Fix | Description |
|-----|-------------|
| Password hashing | 16-bit salt, 5 iterations, `"SSSS+IHHHHHHHH"` format with lazy migration |
| Write-ahead journal | Journal sector at LBA 99, `SAVING`/`CLEAN` states, prevents corruption |
| Block-based FS | Dynamic kmalloc content, block-chain disk storage, 4096-sector image |
| RTL8139 NIC | PCI probe, init, TX/RX, ICMP echo-request/reply |
| Real ping | Ethernet+IP+ICMP frame, QEMU gateway (10.0.2.2), timing |
| exec command | ELF binary loading, user page directory, new scheduler task |
| SMP note | Explicitly documented as unsupported in neofetch |
| Version bump | All banners, help strings, neofetch, login screen, uname updated to 6.3 |

## New Features (v6.3)

| Feature | Description |
|---------|-------------|
| exec | Load ELF binaries from filesystem and spawn as user tasks |
| ping (real) | ICMP echo over RTL8139 NIC — built from scratch |
| RTL8139 driver | PCI RTL8139 NIC with TX/RX ring, MAC detection, init |
| Block-based FS | Dynamic file content via kmalloc, block-chain disk persistence |
| Write-ahead journal | Atomic file table saves with journal state recovery |
| Password hardening | 16-bit salt, 5 hash iterations, old-format migration |

## Verification

Build: `make clean && make`
Run: `make run` (or `qemu-system-i386 -fda os.img -hda storage.img -boot order=a -nographic -netdev user,id=u1 -device rtl8139,netdev=u1`)

Test sequence:
1. Login as `root` / `root`
2. Try commands: `help`, `echo`, `date`, `cpuinfo`, `lspci`, `touch`, `write`, `cat`, `ls`, `uptime`, `free`, `sysinfo`
3. Install games: `diese ayo add games` then play `tetris`, `snake`, `tictactoe`
4. Test file ops: `touch test.txt`, `write test.txt hello`, `cat test.txt`, `wc test.txt`, `hexdump test.txt`, `du`, `rev test.txt`, `shasum test.txt`
5. Test new v6.3 features:
   - NIC: `lspci` should show RTL8139, `ping 10.0.2.2` should get replies
   - Block FS: create files > 2006 bytes, they should persist on reboot
   - Password migration: old-format password hashes get auto-upgraded on login
   - exec: write a small ELF binary to a file, run `exec <filename>`
6. Test old utils: `clock`, `factor 12345`, `watch date`
7. Verify neofetch shows SMP: not supported
8. Verify no crashes or panics from normal operation

## TODO (Future Work)

1. Make shell a proper scheduler task for true multitasking
2. Implement `fork()` syscall for process creation
3. Proper VFS layer with mountable filesystems
4. Add `kmalloc` with slab allocation for better performance
5. Implement demand paging (load pages on fault)
6. Add user-space signal handling
7. SMP support (multi-core scheduling) — blocking for now
