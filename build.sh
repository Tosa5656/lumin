mkdir -p iso/boot
nasm -f elf32 src/boot/boot.asm -o iso/boot/boot.o
ld -m elf_i386 -Ttext 0x100000 -Tdata 0x200000 -Tbss 0x300000 -nostdlib --build-id=none iso/boot/boot.o -o iso/boot/kernel.bin