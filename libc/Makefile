AS = nasm
CC = x86_64-pc-linux-gnu-gcc
AR = ar
CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone -mcmodel=large -I include

CRT_OBJ = obj/crt0.o
LIBC_OBJS = obj/syscall.o obj/errno/errno.o obj/stdlib/exit.o obj/stdlib/malloc.o \
            obj/string/mem.o obj/string/str.o obj/stdio/printf.o

all: libc.a

mkdirs:
	mkdir -p obj obj/errno obj/stdlib obj/string obj/stdio

obj/crt0.o: crt/crt0.asm
	$(AS) -f elf64 $< -o $@

obj/syscall.o: src/syscall.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/errno/errno.o: src/errno/errno.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdlib/exit.o: src/stdlib/exit.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdlib/malloc.o: src/stdlib/malloc.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/string/mem.o: src/string/mem.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/string/str.o: src/string/str.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdio/printf.o: src/stdio/printf.c
	$(CC) $(CFLAGS) -c $< -o $@

libc.a: mkdirs $(CRT_OBJ) $(LIBC_OBJS)
	$(AR) rcs libc.a $(LIBC_OBJS)

clean:
	rm -rf obj libc.a

.PHONY: all clean mkdirs
