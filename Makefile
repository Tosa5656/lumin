all: kernel

AS = nasm
CC = x86_64-pc-linux-gnu-gcc
LD = x86_64-pc-linux-gnu-gcc
CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone
LDFLAGS = -m64 -nostdlib -Wl,-T,kernel/kernel.ld,--oformat,binary
TRUNCATE = truncate
CAT = cat

clean:
	rm -rf bin obj kernel.bin

mkdirs:
	mkdir -p bin obj

bootloader: mkdirs
	${AS} -f bin arch/x86_64/bootloader.asm -o bin/bootloader.bin

kernel: mkdirs bootloader
	${AS} -f elf64 kernel/kernel_entry.asm -o obj/kernel_entry.o
	${AS} -f elf64 kernel/interrupts.asm -o obj/interrupts.o
	${CC} ${CFLAGS} -c kernel/kernel.c -o obj/kernel.o
	${CC} ${CFLAGS} -c kernel/drivers/vga/vga.c -o obj/vga.o
	${CC} ${CFLAGS} -c kernel/drivers/timer/timer.c -o obj/timer.o
	${CC} ${CFLAGS} -c kernel/drivers/timer/pit.c -o obj/pit.o
	${CC} ${CFLAGS} -c kernel/drivers/timer/hpet.c -o obj/hpet.o
	${CC} ${CFLAGS} -c kernel/drivers/timer/lapic.c -o obj/lapic.o
	${CC} ${CFLAGS} -c kernel/kprintf.c -o obj/kprintf.o
	${CC} ${CFLAGS} -c kernel/drivers/serial/serial.c -o obj/serial.o
	${CC} ${CFLAGS} -c kernel/drivers/rtc/rtc.c -o obj/rtc.o
	${CC} ${CFLAGS} -c kernel/idt.c -o obj/idt.o
	${CC} ${CFLAGS} -c kernel/keyboard.c -o obj/keyboard.o
	${LD} ${LDFLAGS} obj/kernel_entry.o obj/interrupts.o obj/kernel.o obj/vga.o obj/kprintf.o obj/serial.o obj/rtc.o obj/timer.o obj/pit.o obj/hpet.o obj/lapic.o obj/idt.o obj/keyboard.o -o bin/kernel.bin
	${TRUNCATE} -s 13312 bin/kernel.bin

	${CAT} bin/bootloader.bin bin/kernel.bin > kernel.bin

qemu: kernel
	qemu-system-x86_64 kernel.bin