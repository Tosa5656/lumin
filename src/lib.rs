#![no_std]
#![no_main]

#![feature(abi_x86_interrupt)]
#![feature(slice_ptr_get)]

pub mod vga;
pub mod gdt;
pub mod interrupts;

use core::panic::PanicInfo;

pub fn init()
{
    gdt::init();
    interrupts::init_idt();


    unsafe { interrupts::PICS.lock().initialize(); }
    x86_64::instructions::interrupts::enable();
}

pub fn enable_interrupts()
{
    x86_64::instructions::interrupts::enable();
}

pub fn hlt() -> !
{
    loop
    {
        x86_64::instructions::hlt();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn kernel_entry(magic: u64, mb_info_addr: u64) -> !
{
    let mb_info_ptr = mb_info_addr as *const u8;

    init();

    hlt();
}

#[panic_handler]
fn panic(info: &PanicInfo) -> !
{
    println!("{}", info);
    hlt();
}