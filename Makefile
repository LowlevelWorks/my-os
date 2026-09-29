CC   := x86_64-elf-gcc
LD   := x86_64-elf-ld
GRUB := i686-elf-grub-mkrescue
QEMU := qemu-system-x86_64

BUILD_DIR := build
KERNEL    := $(BUILD_DIR)/kernel.elf
ISO       := $(BUILD_DIR)/os.iso


# ==========================================
# Compiler flags
# ==========================================

CFLAGS := \
	-std=gnu11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pie \
	-mno-red-zone \
	-Wall \
	-Wextra \
	-Ikernel \
	-Ikernel/shell \
	-Ikernel/memory \
	-Ikernel/arch/x86_64 \
	-MMD \
	-MP  \
	-mcmodel=kernel


# ==========================================
# Source files
# ==========================================

C_SOURCES := \
	kernel/kernel.c \
	kernel/arch/x86_64/interrupts.c \
	kernel/arch/x86_64/pic.c   \
	kernel/drivers/keyboard/keyboard.c \
	kernel/drivers/keyboard/keyboard_map.c \
	kernel/drivers/terminal/terminal.c \
	kernel/shell/shell.c \
	kernel/shell/parser.c \
        kernel/lib/string.c   \
	kernel/shell/commands/commands.c \
	kernel/shell/commands/help.c \
	kernel/shell/commands/echo.c \
	kernel/shell/commands/clear.c \
	kernel/shell/history.c        \
	kernel/memory/physical_memory/physical_memory.c \
	kernel/memory/frame/frame.c          \
	kernel/memory/memblock/memblock.c   \
	kernel/memory/buddy/buddy.c     \
	kernel/arch/x86_64/cpu.c  \
	kernel/memory/panic.c    \
        kernel/memory/paging/paging.c  \
        kernel/memory/paging/paging_utils.c



ASM_SOURCES := \
	boot/boot.S \
	kernel/arch/x86_64/interrupts_handlers.S


# ==========================================
# Object files
# ==========================================

C_OBJECTS := $(C_SOURCES:%.c=$(BUILD_DIR)/%.o)

ASM_OBJECTS := $(ASM_SOURCES:%.S=$(BUILD_DIR)/%.o)

OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)


# ==========================================
# Default target
# ==========================================

all: $(KERNEL)


# ==========================================
# Compile C files
# ==========================================

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


# ==========================================
# Assemble .S files
# ==========================================

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


# ==========================================
# Link kernel
# ==========================================

$(KERNEL): $(OBJECTS) linker.ld
	@mkdir -p $(dir $@)
	$(LD) -T linker.ld -o $@ $(OBJECTS)

# ==========================================
# Create bootable ISO
# ==========================================

iso: $(KERNEL)
	@mkdir -p $(BUILD_DIR)/isodir/boot/grub

	cp $(KERNEL) $(BUILD_DIR)/isodir/boot/kernel.elf
	cp iso/grub/grub.cfg $(BUILD_DIR)/isodir/boot/grub/grub.cfg

	$(GRUB) -o $(ISO) $(BUILD_DIR)/isodir


# ==========================================
# Run OS with QEMU
# ==========================================

run: iso
	$(QEMU) -cdrom $(ISO)
debug: iso
	$(QEMU) -cdrom $(ISO) -d int,cpu_reset,guest_errors  -D qemu.log -no-reboot -no-shutdown


# ==========================================
# Clean build files
# ==========================================

clean:
	rm -rf $(BUILD_DIR)


# ==========================================
# Phony targets
# ==========================================

.PHONY: all iso run clean
