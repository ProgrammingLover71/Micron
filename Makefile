# -----------------------------
# Toolchain
# -----------------------------

TOOLCHAIN := C:/i686-elf-tools-windows/bin

CC      := $(TOOLCHAIN)/i686-elf-gcc.exe
LD      := $(TOOLCHAIN)/i686-elf-ld.exe
OBJCOPY := $(TOOLCHAIN)/i686-elf-objcopy.exe
NASM    := nasm
QEMU    := qemu-system-x86_64

OUT_IMG := micron.img
FAT_IMG := fat32.img
PYTHON  := python

# -----------------------------
# Flags
# -----------------------------

CFLAGS  := -m32 -ffreestanding -fno-stack-protector -fno-pic -nostdlib -Wall -Wextra
LDFLAGS := -T linker.ld

# -----------------------------
# Sources
# -----------------------------

KERNEL_C := $(wildcard src/kernel/*.c)
KBD_C    := $(wildcard src/kbd/*.c)
VGA_C    := $(wildcard src/vga/*.c)
SHELL_C  := $(wildcard src/shell/*.c)
RT_C     := $(wildcard src/rt/*.c)
RT_EDIT_C := $(wildcard src/rt/edit/*.c)
FS_C     := $(wildcard src/fs/*.c)

C_SRCS := $(KERNEL_C) $(KBD_C) $(VGA_C) $(SHELL_C) $(RT_C) $(RT_EDIT_C) $(FS_C)

C_OBJS := \
	$(patsubst src/kernel/%.c,build/%.o,$(KERNEL_C)) \
	$(patsubst src/kbd/%.c,build/%.o,$(KBD_C)) \
	$(patsubst src/vga/%.c,build/%.o,$(VGA_C)) \
	$(patsubst src/shell/%.c,build/%.o,$(SHELL_C)) \
	$(patsubst src/rt/%.c,build/%.o,$(RT_C)) \
	$(patsubst src/rt/edit/%.c,build/edit_%.o,$(RT_EDIT_C)) \
	$(patsubst src/fs/%.c,build/%.o,$(FS_C)) \

ASM_OBJS := build/entry.o

OBJS := $(ASM_OBJS) $(C_OBJS)

.PHONY: all run clean fat32

all: build/img build/$(FAT_IMG)

# -----------------------------
# Build directory
# -----------------------------

build:
	if not exist build mkdir build

# -----------------------------
# Boot sector
# -----------------------------

build/boot.bin: src/boot.s | build
	$(NASM) -f bin $< -o $@

# -----------------------------
# Kernel entry
# -----------------------------

build/entry.o: src/kernel/entry.s | build
	$(NASM) -f elf32 $< -o $@

# -----------------------------
# Compile C
# -----------------------------

build/%.o: src/kernel/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/kbd/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/vga/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/shell/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/rt/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/edit_%.o: src/rt/edit/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/fs/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------
# Link
# -----------------------------

build/kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# -----------------------------
# ELF -> BIN
# -----------------------------

build/kernel.bin: build/kernel.elf
	$(OBJCOPY) -O binary $< $@

# -----------------------------
# Disk image
# -----------------------------

build/img: build/boot.bin build/kernel.bin | build
	copy /b build\boot.bin+build\kernel.bin build\$(OUT_IMG) > nul
	fsutil file seteof build\$(OUT_IMG) 65536 > nul

build/$(FAT_IMG): tools/make_fat32_image.py | build
	$(PYTHON) tools/make_fat32_image.py build/$(FAT_IMG)

fat32: build/$(FAT_IMG)

# -----------------------------
# Run
# -----------------------------

run: build/img build/$(FAT_IMG)
	dir build
	$(QEMU) -drive if=ide,index=0,media=disk,format=raw,file=build/$(OUT_IMG) -drive if=ide,index=1,media=disk,format=raw,file=build/$(FAT_IMG)

# -----------------------------
# Clean
# -----------------------------

clean:
	if exist build rmdir /s /q build
