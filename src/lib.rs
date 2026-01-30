#![no_std]
#![no_main]

pub mod vga;

use core::panic::PanicInfo;

#[unsafe(no_mangle)]
pub extern "C" fn kernel_entry() -> !
{
    println!("Hello, World!");

    loop {}
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> !
{
    loop {}
}