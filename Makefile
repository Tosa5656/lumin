all: kernel

AS = nasm
CC = x86_64-pc-linux-gnu-gcc
LD = x86_64-pc-linux-gnu-gcc
CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone

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
	${CC} ${CFLAGS} -c kernel/idt.c -o obj/idt.o
	${CC} ${CFLAGS} -c kernel/keyboard.c -o obj/keyboard.o
	${LD} -m64 -nostdlib -Wl,-Ttext,0x100000,--oformat,binary obj/kernel_entry.o obj/interrupts.o obj/kernel.o obj/vga.o obj/idt.o obj/keyboard.o -o bin/kernel.bin
	truncate -s 5120 bin/kernel.bin

	cat bin/bootloader.bin bin/kernel.bin > kernel.bin

qemu: kernel
	qemu-system-x86_64 kernel.bin