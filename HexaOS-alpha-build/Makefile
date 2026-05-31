SHELL := /bin/bash

CC = gcc
CFLAGS = -m32 -ffreestanding -nostdinc -nostdlib -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -Wall -Wextra -I.
LDFLAGS = -T link.ld -m elf_i386 -nostdlib

OBJECTS = kernel_entry.o interrupts.o paging.o scheduler.o syscall.o hexa.o

all: clean os.img storage.img

boot.bin: boot_entry.asm kernel.bin
	nasm -f bin -o boot.bin boot_entry.asm
	# Patch boot.bin at offset 27 with actual kernel sector count (mov si, imm8)
	ksect=$$(( $$(stat -c '%s' kernel.bin) / 512 + 1 )); \
	[ $$ksect -gt 254 ] && ksect=254; \
	printf '%b' "\\x$$(printf '%02x' $$ksect)" | dd of=boot.bin bs=1 seek=27 count=1 conv=notrunc 2>/dev/null; \
	echo "[*] Kernel sectors: $$ksect"

kernel_entry.o: kernel_entry.asm
	nasm -f elf32 -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJECTS) link.ld
	ld $(LDFLAGS) -o $@ $(OBJECTS) -Map=link.map

kernel.bin: kernel.elf
	objcopy -O binary $< $@

storage.img:
	dd if=/dev/zero bs=512 count=256 of=storage.img 2>/dev/null

os.img: boot.bin kernel.bin
	cat boot.bin kernel.bin > os.img
	@truncate -s 1474560 os.img
	@echo "[✓] OS image: $@"

run: os.img storage.img
	@echo "[*] Booting HEXA OS in QEMU..."
	@qemu-system-i386 -fda os.img -hda storage.img -boot order=a -nographic

clean:
	rm -f *.o *.elf *.bin os.img storage.img
	@echo "[✓] Cleaned."

.PHONY: all run clean
