AS = nasm
CC = x86_64-pc-linux-gnu-gcc
AR = ar
CFLAGS = -m64 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone -mcmodel=large -I include

CRT_OBJ = obj/crt0.o
LIBC_OBJS = obj/errno/errno.o obj/stdlib/exit.o obj/stdlib/malloc.o \
            obj/stdlib/strtol.o obj/stdlib/rand.o obj/stdlib/qsort.o obj/stdlib/abs.o \
            obj/string/mem.o obj/string/str.o obj/stdio/printf.o \
            obj/stdio/file.o obj/stdio/fget.o obj/stdio/fput.o obj/stdio/scanf.o \
            obj/unistd/unistd.o obj/signal/signal.o obj/dirent/dirent.o \
            obj/stat.o obj/wait.o

all: libc.a

mkdirs:
	mkdir -p obj obj/errno obj/stdlib obj/string obj/stdio obj/unistd obj/signal obj/dirent

obj/crt0.o: crt/crt0.asm
	$(AS) -f elf64 $< -o $@

obj/errno/errno.o: src/errno/errno.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdlib/exit.o: src/stdlib/exit.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdlib/malloc.o: src/stdlib/malloc.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdlib/strtol.o: src/stdlib/strtol.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdlib/rand.o: src/stdlib/rand.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdlib/qsort.o: src/stdlib/qsort.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdlib/abs.o: src/stdlib/abs.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/string/mem.o: src/string/mem.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/string/str.o: src/string/str.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdio/printf.o: src/stdio/printf.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdio/file.o: src/stdio/file.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdio/fget.o: src/stdio/fget.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdio/fput.o: src/stdio/fput.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stdio/scanf.o: src/stdio/scanf.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/unistd/unistd.o: src/unistd/unistd.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/signal/signal.o: src/signal/signal.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/dirent/dirent.o: src/dirent/dirent.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/stat.o: src/stat.c
	$(CC) $(CFLAGS) -c $< -o $@

obj/wait.o: src/wait.c
	$(CC) $(CFLAGS) -c $< -o $@

libc.a: mkdirs $(CRT_OBJ) $(LIBC_OBJS)
	$(AR) rcs libc.a $(LIBC_OBJS)

clean:
	rm -rf obj libc.a

.PHONY: all clean mkdirs
