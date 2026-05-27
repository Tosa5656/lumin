all: kernel

AS = nasm
CC = x86_64-pc-linux-gnu-gcc
LD = x86_64-pc-linux-gnu-gcc
CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone -I kernel -I os
LDFLAGS = -m64 -nostdlib -Wl,-T,kernel/kernel.ld,--oformat,binary
TRUNCATE = truncate
CAT = cat

clean:
	rm -rf bin obj kernel.bin
	$(MAKE) -C libc clean

mkdirs:
	mkdir -p bin obj

bootloader: mkdirs
	${AS} -f bin arch/x86_64/bootloader.asm -o bin/bootloader.bin

HDRS = kernel os

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
obj/acpi.o: kernel/drivers/acpi/acpi.c
	${CC} ${CFLAGS} -c $< -o $@
obj/gdt.o: kernel/gdt.c
	${CC} ${CFLAGS} -c $< -o $@

obj/idt.o: kernel/idt.c
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

obj/shell.o: os/shell.c
	${CC} ${CFLAGS} -c $< -o $@
obj/keyboard.o: os/keyboard.c
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
obj/ata.o: kernel/drivers/ata/ata.c
	${CC} ${CFLAGS} -c $< -o $@
obj/block.o: kernel/block/block.c
	${CC} ${CFLAGS} -c $< -o $@
obj/vfs.o: kernel/fs/vfs.c
	${CC} ${CFLAGS} -c $< -o $@
obj/fat32.o: kernel/fs/fat32.c
	${CC} ${CFLAGS} -c $< -o $@

OBJS = obj/kernel_entry.o obj/interrupts.o obj/kernel.o obj/kprintf.o \
       obj/panic.o obj/acpi.o obj/gdt.o obj/idt.o obj/pmm.o obj/kmalloc.o \
       obj/vmm.o obj/elf.o obj/task.o obj/syscall.o \
       obj/shell.o obj/keyboard.o obj/vga.o obj/serial.o obj/rtc.o \
       obj/timer.o obj/pit.o obj/hpet.o obj/lapic.o \
       obj/pci.o obj/ata.o obj/block.o obj/vfs.o obj/fat32.o

kernel: mkdirs bootloader $(OBJS)
	${LD} ${LDFLAGS} $(OBJS) -o bin/kernel.bin
	${TRUNCATE} -s 102400 bin/kernel.bin
	${CAT} bin/bootloader.bin bin/kernel.bin > lumin.bin

USER_CC = x86_64-pc-linux-gnu-gcc
USER_CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone -mcmodel=large -I libc/include
USER_LDFLAGS = -m64 -no-pie -nostdlib -mcmodel=large -Wl,-Ttext-segment=0x8000000000,-e,_start

LIBC = libc/libc.a
LIBC_CRT0 = libc/obj/crt0.o

libc/libc.a:
	$(MAKE) -C libc

user/init.elf: user/init.c $(LIBC)
	${USER_CC} ${USER_CFLAGS} ${USER_LDFLAGS} -o $@ $(LIBC_CRT0) $< -L libc -lc
	chmod -x $@

user/hello.elf: user/hello.c $(LIBC)
	${USER_CC} ${USER_CFLAGS} ${USER_LDFLAGS} -o $@ $(LIBC_CRT0) $< -L libc -lc
	chmod -x $@

user/shell.elf: user/shell.c $(LIBC)
	${USER_CC} ${USER_CFLAGS} ${USER_LDFLAGS} -o $@ $(LIBC_CRT0) $< -L libc -lc
	chmod -x $@

fat32.img: user/init.elf user/hello.elf user/shell.elf
	dd if=/dev/zero of=fat32.img bs=1M count=32
	mkfs.fat -F 32 fat32.img
	mcopy -i fat32.img user/init.elf ::init.elf
	mcopy -i fat32.img user/hello.elf ::hello.elf
	mcopy -i fat32.img user/shell.elf ::shell.elf

qemu: kernel fat32.img
	qemu-system-x86_64 -drive format=raw,file=lumin.bin \
						-drive format=raw,file=fat32.img \
						-serial mon:stdio