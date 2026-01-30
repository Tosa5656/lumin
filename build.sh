mkdir -p {obj,iso}
nasm -f elf64 src/bootloader.asm -o obj/bootloader.o
cargo build --release --target x86_64-unknown-none
ld -n -T linker.ld -o iso/kernel.bin obj/bootloader.o target/x86_64-unknown-none/release/liblumin.a