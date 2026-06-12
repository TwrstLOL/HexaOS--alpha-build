# HexaOS — Kernel Infrastructure Review

## Status: v6.1 "VFS Edition" — Clock and Tetris fixed

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

### ✅ Persistence
- ATA PIO block device driver (primary channel, LBA28)
- Read/write sectors (proven working — file system loads/stores)
- Storage persists via `storage.img` (QEMU virtual HDD)
- **v6.0**: File content expanded from 512 to 2006 bytes (4 ATA sectors per file)
- **v6.1**: Version bumped to 6.1, all docs and banners updated

### ✅ Control loop
- Page faults are logged and recovered (system continues)
- GPF is logged and recovered
- Other exceptions are logged and recovered
- Double fault halts (unrecoverable by definition)

### ✅ Shell & Commands (90+)
- **v6.0**: Added 12+ new commands: `clock`, `free`, `ping`, `factor`, `hexdump`, `du`, `rev`, `shasum`, `sysinfo`, `watch`, `tetris`
- **v6.1**: `clock` rewritten (direct VGA+serial), `tetris` rebuilt with correct bitmask shape data
- Fixed IRQ keyboard arrow key support for history navigation

## Recent Changes (v6.1)

| Fix | Description |
|-----|-------------|
| Clock display | Rewritten — no more "all on one line" garbling; uses direct VGA buffer + serial \r |
| Tetris game | Rebuilt with uint16 bitmask shape data — pieces now collide, rotate, lock properly |
| Version bump | All banners, help strings, neofetch, login screen, uname updated to 6.1 |

## New Features (v6.1)

| Feature | Description |
|---------|-------------|
| Tetris (rebuilt v6.1) | Full Tetris game — bitmask shapes, correct collision/rotation/locking |
| clock (fixed v6.1) | Live RTC clock display with working serial+VGA output |
| free | Memory statistics showing PMM, tasks, files, users |
| ping | Simulated network ping with 4-packet stats |
| factor | Prime factorization of any integer |
| hexdump | Hex + printable ASCII dump of any file |
| du | Disk usage by file |
| rev | Reverse file content |
| shasum | DJB2 hash of file |
| sysinfo | Quick system overview |
| watch | Repeat any command at intervals |

## Verification

Build: `make clean && make`
Run: `make run` (or `qemu-system-i386 -fda os.img -hda storage.img -boot order=a -nographic`)

Test sequence:
1. Login as `root` / `root`
2. Try commands: `help`, `echo`, `date`, `cpuinfo`, `lspci`, `touch`, `write`, `cat`, `ls`, `uptime`, `free`, `sysinfo`
3. Install games: `diese ayo add games` then play `tetris`, `snake`, `tictactoe`
4. Test file ops: `touch test.txt`, `write test.txt hello`, `cat test.txt`, `wc test.txt`, `hexdump test.txt`, `du`, `rev test.txt`, `shasum test.txt`
5. Test new utils: `clock`, `ping localhost`, `factor 12345`, `watch date`
6. Verify no crashes or panics from normal operation

## TODO (Future Work)

1. Make shell a proper scheduler task for true multitasking
2. Wire up `enter_userspace` to run user tasks in ring 3
3. Implement `exec()` syscall to load user programs from filesystem
4. Add `fork()` syscall for process creation
5. Proper VFS layer with mountable filesystems
6. Add `kmalloc` with slab allocation for better performance
7. Implement demand paging (load pages on fault)
8. Add user-space signal handling
9. ELF loader integration (loader exists but not wired to exec)
10. SMP support (multi-core scheduling)
