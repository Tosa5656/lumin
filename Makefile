all: kernel

AS = nasm
CC = x86_64-pc-linux-gnu-gcc
LD = x86_64-pc-linux-gnu-gcc
CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone -I kernel -I os
LDFLAGS = -m64 -nostdlib -Wl,-T,kernel/kernel.ld,--oformat,binary
KERNEL_SECTORS = 216
KERNEL_MAX = $$(( $(KERNEL_SECTORS) * 512 - $(STAGE2_SIZE) ))
TRUNCATE = truncate
CAT = cat
STAGE2_SIZE = 4096

-include .config

AUTOCONF = kernel/include/generated/autoconf.h

$(AUTOCONF):
	python3 tools/kconfig/genconfig.py autoconf

.config:
	python3 tools/kconfig/genconfig.py config

ifneq (,$(wildcard .config))
$(AUTOCONF): .config
CFLAGS += -include $(AUTOCONF)
endif

menuconfig:
	python3 tools/kconfig/genconfig.py menuconfig
	$(MAKE) $(AUTOCONF)

savedefconfig:
	python3 tools/kconfig/genconfig.py defconfig

clean:
	rm -rf bin obj kernel.bin *.elf *.bin user/*.elf user/*/*.elf fat32.img
	$(MAKE) -C libc clean

clean-config:
	rm -f .config .config.old $(AUTOCONF)

distclean: clean clean-config

mkdirs:
	mkdir -p bin obj kernel/include/generated

bin/stage2.bin: arch/x86_64/stage2.asm
	${AS} -f bin $< -o $@

bootloader: mkdirs bin/stage2.bin
	${AS} -f bin arch/x86_64/bootloader.asm -o bin/bootloader.bin

obj/kernel_entry.o: kernel/kernel_entry.asm
	${AS} -f elf64 $< -o $@

obj/interrupts.o: kernel/interrupts.asm
	${AS} -f elf64 $< -o $@

obj/kernel.o: kernel/kernel.c
	${CC} ${CFLAGS} -c $< -o $@
obj/kprintf.o: kernel/kprintf.c
	${CC} ${CFLAGS} -c $< -o $@
obj/panic.o: kernel/panic.c
	${CC} ${CFLAGS} -c $< -o $@
obj/gdt.o: kernel/gdt.c
	${CC} ${CFLAGS} -c $< -o $@
obj/idt.o: kernel/idt.c
	${CC} ${CFLAGS} -c $< -o $@

obj/acpi.o: kernel/drivers/acpi/acpi.c
	${CC} ${CFLAGS} -c $< -o $@
obj/pmm.o: kernel/mm/pmm.c
	${CC} ${CFLAGS} -c $< -o $@
obj/kmalloc.o: kernel/mm/kmalloc.c
	${CC} ${CFLAGS} -c $< -o $@
obj/vmm.o: kernel/mm/vmm.c
	${CC} ${CFLAGS} -c $< -o $@
obj/elf.o: kernel/proc/elf.c
	${CC} ${CFLAGS} -c $< -o $@
obj/task.o: kernel/proc/task.c
	${CC} ${CFLAGS} -c $< -o $@
obj/syscall.o: kernel/proc/syscall.c
	${CC} ${CFLAGS} -c $< -o $@
obj/keyboard.o: os/keyboard.c
	${CC} ${CFLAGS} -c $< -o $@
obj/initcall.o: kernel/include/initcall.c
	${CC} ${CFLAGS} -c $< -o $@
obj/smp.o: kernel/smp/smp.c
	${CC} ${CFLAGS} -c $< -o $@
obj/pipefs.o: kernel/fs/pipefs.c
	${CC} ${CFLAGS} -c $< -o $@
obj/device.o: kernel/drivers/device.c
	${CC} ${CFLAGS} -c $< -o $@
obj/procfs.o: kernel/fs/procfs.c
	${CC} ${CFLAGS} -c $< -o $@
obj/tmpfs.o: kernel/fs/tmpfs.c
	${CC} ${CFLAGS} -c $< -o $@
obj/vga.o: kernel/drivers/vga/vga.c
	${CC} ${CFLAGS} -c $< -o $@
obj/serial.o: kernel/drivers/serial/serial.c
	${CC} ${CFLAGS} -c $< -o $@
obj/rtc.o: kernel/drivers/rtc/rtc.c
	${CC} ${CFLAGS} -c $< -o $@
obj/timer.o: kernel/drivers/timer/timer.c
	${CC} ${CFLAGS} -c $< -o $@
obj/pit.o: kernel/drivers/timer/pit.c
	${CC} ${CFLAGS} -c $< -o $@
obj/hpet.o: kernel/drivers/timer/hpet.c
	${CC} ${CFLAGS} -c $< -o $@
obj/lapic.o: kernel/drivers/timer/lapic.c
	${CC} ${CFLAGS} -c $< -o $@
obj/pci.o: kernel/drivers/pci/pci.c
	${CC} ${CFLAGS} -c $< -o $@
obj/fb.o: kernel/drivers/fb/fb.c kernel/drivers/fb/fb.h kernel/drivers/fb/fb_font.h
	${CC} ${CFLAGS} -c $< -o $@
obj/ata.o: kernel/drivers/ata/ata.c
	${CC} ${CFLAGS} -c $< -o $@
obj/block.o: kernel/block/block.c
	${CC} ${CFLAGS} -c $< -o $@
obj/vfs.o: kernel/fs/vfs.c
	${CC} ${CFLAGS} -c $< -o $@
obj/fat32.o: kernel/fs/fat32.c
	${CC} ${CFLAGS} -c $< -o $@

CORE_OBJS = obj/kernel_entry.o obj/interrupts.o \
            obj/kernel.o obj/kprintf.o obj/panic.o \
            obj/gdt.o obj/idt.o

ifeq ($(CONFIG_ACPI),y)
OBJS_ACPI = obj/acpi.o
endif

ifeq ($(CONFIG_PMM),y)
OBJS_PMM = obj/pmm.o
endif
ifeq ($(CONFIG_KMALLOC),y)
OBJS_KMALLOC = obj/kmalloc.o
endif
ifeq ($(CONFIG_VMM),y)
OBJS_VMM = obj/vmm.o
endif
ifeq ($(CONFIG_ELF),y)
OBJS_ELF = obj/elf.o
endif
ifeq ($(CONFIG_TASKS),y)
OBJS_TASK = obj/task.o
endif
ifeq ($(CONFIG_SYSCALL),y)
OBJS_SYSCALL = obj/syscall.o
endif
ifeq ($(CONFIG_KEYBOARD),y)
OBJS_KEYBOARD = obj/keyboard.o
endif
ifeq ($(CONFIG_VGA),y)
OBJS_VGA = obj/vga.o
endif
ifeq ($(CONFIG_SERIAL),y)
OBJS_SERIAL = obj/serial.o
endif
ifeq ($(CONFIG_RTC),y)
OBJS_RTC = obj/rtc.o
endif
ifeq ($(CONFIG_PCI),y)
OBJS_PCI = obj/pci.o
endif
ifeq ($(CONFIG_FB),y)
OBJS_FB = obj/fb.o
endif
ifeq ($(CONFIG_ATA),y)
OBJS_ATA = obj/ata.o
endif
ifeq ($(CONFIG_BLOCK),y)
OBJS_BLOCK = obj/block.o
endif
ifeq ($(CONFIG_VFS),y)
OBJS_VFS = obj/vfs.o
endif
ifeq ($(CONFIG_FAT32),y)
OBJS_FAT32 = obj/fat32.o
endif

ifeq (,$(wildcard .config))
    CONFIG_ACPI     := y
    CONFIG_PMM      := y
    CONFIG_KMALLOC  := y
    CONFIG_VMM      := y
    CONFIG_ELF      := y
    CONFIG_TASKS    := y
    CONFIG_SYSCALL  := y
    CONFIG_KEYBOARD := y
    CONFIG_VGA      := y
    CONFIG_SERIAL   := y
    CONFIG_RTC      := y
    CONFIG_PCI      := y
    CONFIG_FB       := y
    CONFIG_ATA      := y
    CONFIG_BLOCK    := y
    CONFIG_VFS      := y
    CONFIG_FAT32    := y
endif

OBJS_TIMER = obj/timer.o

OBJS = $(CORE_OBJS) \
       $(OBJS_ACPI) $(OBJS_PMM) $(OBJS_KMALLOC) $(OBJS_VMM) \
       $(OBJS_ELF) $(OBJS_TASK) $(OBJS_SYSCALL) \
       $(OBJS_KEYBOARD) $(OBJS_VGA) $(OBJS_SERIAL) $(OBJS_RTC) \
       $(OBJS_TIMER) obj/pit.o obj/hpet.o obj/lapic.o \
       $(OBJS_PCI) $(OBJS_FB) $(OBJS_ATA) \
       $(OBJS_BLOCK) $(OBJS_VFS) $(OBJS_FAT32) \
       obj/initcall.o obj/smp.o obj/pipefs.o obj/device.o obj/procfs.o obj/tmpfs.o

kernel: mkdirs bootloader bin/stage2.bin $(OBJS) $(AUTOCONF)
	${LD} ${LDFLAGS} $(OBJS) -o bin/kernel.bin
	${TRUNCATE} -s $(KERNEL_MAX) bin/kernel.bin
	${TRUNCATE} -s $(STAGE2_SIZE) bin/stage2.bin
	${CAT} bin/bootloader.bin bin/stage2.bin bin/kernel.bin > lumin.bin

USER_CC = x86_64-pc-linux-gnu-gcc
USER_CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone -mcmodel=large -I libc/include
USER_LDFLAGS = -m64 -no-pie -nostdlib -mcmodel=large -Wl,-Ttext-segment=0x8000000000,-e,_start

LIBC = libc/libc.a
LIBC_CRT0 = libc/obj/crt0.o

libc/libc.a:
	$(MAKE) -C libc

ifeq (,$(wildcard .config))
    CONFIG_HELLO     := y
    CONFIG_SHELL     := y
    CONFIG_AS        := y
    CONFIG_ECC       := y
    CONFIG_COREUTILS := y
endif

USER_ELFS = $(and $(CONFIG_HELLO),user/hello.elf) \
            $(and $(CONFIG_SHELL),user/shell.elf) \
            $(and $(CONFIG_COREUTILS),$(CORETIL_ELFS))

CORETILS = ls cat echo clear help
CORETIL_ELFS = $(addprefix user/coreutils/,$(addsuffix .elf,$(CORETILS)))

ifeq ($(CONFIG_HELLO),y)
user/hello.elf: user/hello.c $(LIBC)
	${USER_CC} ${USER_CFLAGS} ${USER_LDFLAGS} -o $@ $(LIBC_CRT0) $< -L libc -lc
	chmod -x $@
endif

ifeq ($(CONFIG_SHELL),y)
user/shell.elf: user/shell.c $(LIBC)
	${USER_CC} ${USER_CFLAGS} ${USER_LDFLAGS} -o $@ $(LIBC_CRT0) $< -L libc -lc
	chmod -x $@
endif

ifeq ($(CONFIG_COREUTILS),y)
user/coreutils/%.elf: user/coreutils/%.c $(LIBC)
	${USER_CC} ${USER_CFLAGS} ${USER_LDFLAGS} -o $@ $(LIBC_CRT0) $< -L libc -lc
	chmod -x $@
endif

ALL_ELFS = $(USER_ELFS)

fat32.img: $(ALL_ELFS)
	dd if=/dev/zero of=fat32.img bs=1M count=32
	mkfs.fat -F 32 fat32.img
	mmd -i fat32.img ::/system
	mmd -i fat32.img ::/system/bin
	for f in $(ALL_ELFS); do \
		mcopy -i fat32.img $$f ::/system/bin/$$(basename $$f); \
	done

qemu: kernel fat32.img
	qemu-system-x86_64 -drive format=raw,file=lumin.bin \
						-drive format=raw,file=fat32.img \
						-serial mon:stdio

.PHONY: all clean clean-config distclean menuconfig savedefconfig \
        mkdirs bootloader kernel qemu