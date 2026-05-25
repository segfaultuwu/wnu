PROJECT := wnu
VERSION := 0.1.0

ARCH ?= x86_64

BUILD_DIR := build
ISO_ROOT := iso_root
ISO_IMAGE := $(PROJECT).iso

LIMINE_DIR ?= /home/segfault/limine-bin
LIMINE := $(LIMINE_DIR)/limine

CC := gcc
ASM := nasm
LD := ld
OBJDUMP := objdump
NM := nm
XORRISO := xorriso
QEMU := qemu-system-x86_64
GDB := gdb

CFLAGS := \
	-std=c11 \
	-O2 \
	-g \
	-Wall \
	-Wextra \
	-Wpedantic \
	-MMD \
	-MP \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-fno-pie \
	-fno-omit-frame-pointer \
	-mno-red-zone \
	-mgeneral-regs-only \
	-mcmodel=kernel \
	-Iinclude \
	-I$(LIMINE_DIR)

LDFLAGS := \
	-nostdlib \
	-static \
	-z max-page-size=0x1000 \
	-T linker.ld

QEMUFLAGS := \
	-cdrom $(ISO_IMAGE) \
	-boot order=d \
	-m 512M \
	-serial stdio \
	-display gtk \
	-machine pc \
	-no-reboot \
	-no-shutdown \
	-debugcon file:debug.log \
	-d int,cpu_reset,guest_errors \
	-D qemu.log \
	-netdev user,id=net0 -device rtl8139,netdev=net0

# FAT32 disk image to create in build/ and attach to QEMU as a drive
DISK_IMAGE := $(BUILD_DIR)/disk.img

QEMUFLAGS_KVM := \
	$(QEMUFLAGS) \
	-enable-kvm \
	-cpu host

QEMUFLAGS_DEBUG := \
	$(QEMUFLAGS) \
	-s \
	-S

LIMINE_FILES := \
	$(ISO_ROOT)/boot/limine/limine.conf \
	$(ISO_ROOT)/boot/limine/limine-bios-cd.bin \
	$(ISO_ROOT)/boot/limine/limine-bios.sys \
	$(ISO_ROOT)/boot/limine/limine-uefi-cd.bin \
	$(ISO_ROOT)/EFI/BOOT/BOOTX64.EFI

C_SRCS := $(shell find src -name '*.c')
ASM_SRCS := $(shell find src -name '*.asm')

C_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst src/%.asm,$(BUILD_DIR)/%.o,$(ASM_SRCS))

OBJS := $(C_OBJS) $(ASM_OBJS)
DEPS := $(OBJS:.o=.d)

FONT_OBJ := $(BUILD_DIR)/assets/ter-u16n.o

KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_MAP := $(BUILD_DIR)/kernel.map
KERNEL_SYM := $(BUILD_DIR)/kernel.sym
KERNEL_ASM := $(BUILD_DIR)/kernel.asm

all: $(ISO_IMAGE)

check:
	@command -v $(CC) >/dev/null || { echo "missing: $(CC)"; exit 1; }
	@command -v $(LD) >/dev/null || { echo "missing: $(LD)"; exit 1; }
	@command -v $(XORRISO) >/dev/null || { echo "missing: $(XORRISO)"; exit 1; }
	@command -v $(QEMU) >/dev/null || { echo "missing: $(QEMU)"; exit 1; }
	@test -x "$(LIMINE)" || { echo "missing limine executable: $(LIMINE)"; exit 1; }
	@test -f "$(LIMINE_DIR)/limine-bios-cd.bin" || { echo "missing: $(LIMINE_DIR)/limine-bios-cd.bin"; exit 1; }
	@test -f "$(LIMINE_DIR)/limine-bios.sys" || { echo "missing: $(LIMINE_DIR)/limine-bios.sys"; exit 1; }
	@test -f "$(LIMINE_DIR)/limine-uefi-cd.bin" || { echo "missing: $(LIMINE_DIR)/limine-uefi-cd.bin"; exit 1; }
	@test -f "$(LIMINE_DIR)/BOOTX64.EFI" || { echo "missing: $(LIMINE_DIR)/BOOTX64.EFI"; exit 1; }
	@test -f "limine.conf" || { echo "missing: limine.conf"; exit 1; }
	@test -f "linker.ld" || { echo "missing: linker.ld"; exit 1; }
	@test -f "assets/ter-u16n.psf" || { echo "missing: assets/ter-u16n.psf"; exit 1; }

$(BUILD_DIR):
	mkdir -p $@

$(ISO_ROOT)/boot/limine:
	mkdir -p $@

$(ISO_ROOT)/EFI/BOOT:
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.asm
	@mkdir -p $(dir $@)
	$(ASM) -f elf64 $< -o $@

$(FONT_OBJ): assets/ter-u16n.psf
	@mkdir -p $(dir $@)
	$(LD) -r -b binary $< -o $@

$(KERNEL_ELF): $(OBJS) $(FONT_OBJ) linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -Map=$(KERNEL_MAP) $(OBJS) $(FONT_OBJ) -o $@

$(ISO_ROOT)/kernel: $(KERNEL_ELF) | $(ISO_ROOT)/boot/limine
	cp $< $@

$(ISO_ROOT)/boot/limine/limine.conf: limine.conf | $(ISO_ROOT)/boot/limine
	cp $< $@

$(ISO_ROOT)/boot/limine/limine-bios-cd.bin: | $(ISO_ROOT)/boot/limine
	cp $(LIMINE_DIR)/limine-bios-cd.bin $@

$(ISO_ROOT)/boot/limine/limine-bios.sys: | $(ISO_ROOT)/boot/limine
	cp $(LIMINE_DIR)/limine-bios.sys $@

$(ISO_ROOT)/boot/limine/limine-uefi-cd.bin: | $(ISO_ROOT)/boot/limine
	cp $(LIMINE_DIR)/limine-uefi-cd.bin $@

$(ISO_ROOT)/EFI/BOOT/BOOTX64.EFI: | $(ISO_ROOT)/EFI/BOOT
	cp $(LIMINE_DIR)/BOOTX64.EFI $@

$(ISO_IMAGE): check $(ISO_ROOT)/kernel $(LIMINE_FILES)
	$(XORRISO) -as mkisofs \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label \
		$(ISO_ROOT) \
		-o $@
	$(LIMINE) bios-install $@

$(DISK_IMAGE): | $(BUILD_DIR)
	@echo "Creating FAT32 disk image: $@"
	@dd if=/dev/zero of=$@ bs=1M count=64 >/dev/null 2>&1 || { echo "dd failed"; exit 1; }
	@if command -v mkfs.fat >/dev/null 2>&1; then \
		mkfs.fat -F 32 -n WNU $@ >/dev/null 2>&1 || { echo "mkfs.fat failed"; exit 1; }; \
	elif command -v mkfs.vfat >/dev/null 2>&1; then \
		mkfs.vfat -F 32 -n WNU $@ >/dev/null 2>&1 || { echo "mkfs.vfat failed"; exit 1; }; \
	elif command -v mformat >/dev/null 2>&1; then \
		# mformat requires MTOOLSRC or drive specification; try basic mformat usage \
		mformat -i $@ :: >/dev/null 2>&1 || { echo "mformat failed"; exit 1; }; \
	else \
		echo "no FAT formatter found (mkfs.fat|mkfs.vfat|mformat)"; exit 1; \
	fi

.PHONY: populate-disk
populate-disk: $(DISK_IMAGE)
	@echo "Populating FAT32 disk image: $(DISK_IMAGE)"
	@if command -v mcopy >/dev/null 2>&1; then \
		mcopy -i $(DISK_IMAGE) -s build/disk_files/* ::/ || { echo "mcopy failed"; exit 1; }; \
	elif [ "$(shell id -u)" = "0" ]; then \
		# mount via loop and copy files (requires root) \
		mkdir -p /mnt/wnu_disk && mount -o loop $(DISK_IMAGE) /mnt/wnu_disk || { echo "mount failed"; exit 1; }; \
		cp -r build/disk_files/* /mnt/wnu_disk/ || true; \
		sync; umount /mnt/wnu_disk; rmdir /mnt/wnu_disk; \
	else \
		echo "No suitable tool to populate disk image (need mcopy or root)."; exit 1; \
	fi

run-disk: $(ISO_IMAGE) $(DISK_IMAGE)
	rm -f qemu.log debug.log
	$(QEMU) $(QEMUFLAGS) \
		-drive if=none,file=$(DISK_IMAGE),format=raw,id=hd0 \
		-device ahci,id=ahci \
		-device ide-hd,drive=hd0,bus=ahci.0

run: $(ISO_IMAGE)
	rm -f qemu.log debug.log
	$(QEMU) $(QEMUFLAGS)

run-kvm: $(ISO_IMAGE)
	rm -f qemu.log debug.log
	$(QEMU) $(QEMUFLAGS_KVM)

debug: $(ISO_IMAGE)
	rm -f qemu.log debug.log
	$(QEMU) $(QEMUFLAGS_DEBUG)

gdb: $(KERNEL_ELF)
	$(GDB) $(KERNEL_ELF) \
		-ex "target remote localhost:1234"

symbols: $(KERNEL_ELF)
	$(NM) -n $(KERNEL_ELF) > $(KERNEL_SYM)

objdump: $(KERNEL_ELF)
	$(OBJDUMP) -d $(KERNEL_ELF) > $(KERNEL_ASM)

logs:
	@echo "=== debug.log ==="
	@cat debug.log 2>/dev/null || true
	@echo
	@echo "=== qemu.log tail ==="
	@tail -200 qemu.log 2>/dev/null || true

clean:
	rm -rf $(BUILD_DIR) $(ISO_ROOT) $(ISO_IMAGE) qemu.log debug.log

rebuild: clean all

.PHONY: all check run run-kvm debug gdb symbols objdump logs clean rebuild

-include $(DEPS)
