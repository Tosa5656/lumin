use x86_64::structures::idt::{InterruptDescriptorTable, InterruptStackFrame};
use x86_64::instructions::port::Port;
use pic8259::ChainedPics;
use pc_keyboard::{layouts, DecodedKey, HandleControl, Keyboard, ScancodeSet1};
use spin;
use spin::Mutex;
use lazy_static::lazy_static;
use crate::{gdt, println, print};

#[derive(Debug, Clone, Copy)]
#[repr(u8)]
pub enum InterruptIndex
{
    Timer = PIC_1_OFFSET,
    Keyboard,
}

impl InterruptIndex
{
    fn as_u8(self) -> u8
    {
        self as u8
    }

    fn as_usize(self) -> usize
    {
        usize::from(self.as_u8())
    }
}

// =====================PIC8259=====================
pub const PIC_1_OFFSET: u8 = 32;
pub const PIC_2_OFFSET: u8 = PIC_1_OFFSET + 8;

pub static PICS: spin::Mutex<ChainedPics> = spin::Mutex::new(unsafe { ChainedPics::new(PIC_1_OFFSET, PIC_2_OFFSET) } );

// =====================IDT=========================
lazy_static!
{
    static ref IDT: InterruptDescriptorTable =
    {
        let mut idt = InterruptDescriptorTable::new();
        idt.breakpoint.set_handler_fn(breakpoint_handler);
        
        unsafe
        {
            idt.divide_error.set_handler_fn(divide_error_handler).set_stack_index(gdt::GENERAL_PROTECTION_IST_INDEX);
            idt.invalid_opcode.set_handler_fn(invalid_opcode_handler).set_stack_index(gdt::GENERAL_PROTECTION_IST_INDEX);
            idt.invalid_tss.set_handler_fn(invalid_tss_handler).set_stack_index(gdt::GENERAL_PROTECTION_IST_INDEX);
            idt.segment_not_present.set_handler_fn(segment_not_present_handler).set_stack_index(gdt::GENERAL_PROTECTION_IST_INDEX);
            idt.stack_segment_fault.set_handler_fn(stack_segment_fault_handler).set_stack_index(gdt::GENERAL_PROTECTION_IST_INDEX);
            idt.alignment_check.set_handler_fn(alignment_check_handler).set_stack_index(gdt::GENERAL_PROTECTION_IST_INDEX);
            idt.double_fault.set_handler_fn(double_fault_handler).set_stack_index(gdt::DOUBLE_FAULT_IST_INDEX);
            idt.general_protection_fault.set_handler_fn(general_protection_fault_handler).set_stack_index(gdt::GENERAL_PROTECTION_IST_INDEX);
        }

        idt[InterruptIndex::Timer.as_usize()].set_handler_fn(timer_interrupt_handler);
        idt[InterruptIndex::Keyboard.as_usize()].set_handler_fn(keyboard_interrupt_handler);

        idt
    };
}

pub fn init_idt()
{
    IDT.load();
}
extern "x86-interrupt" fn breakpoint_handler(stack_frame: InterruptStackFrame)
{
    println!("Exception: breakpoint\n{:#?}", stack_frame);
}

extern "x86-interrupt" fn divide_error_handler(stack_frame: InterruptStackFrame)
{
    println!("Exception: divide error");
    println!("{:#?}", stack_frame);
    crate::hlt();
}

extern "x86-interrupt" fn invalid_opcode_handler(stack_frame: InterruptStackFrame)
{
    println!("Exception: invalid opcode");
    println!("{:#?}", stack_frame);
    crate::hlt();
}

extern "x86-interrupt" fn invalid_tss_handler(stack_frame: InterruptStackFrame, error_code: u64)
{
    println!("Exception: invalid TSS");
    println!("Error code: {:#x}", error_code);
    println!("{:#?}", stack_frame);
    crate::hlt();
}

extern "x86-interrupt" fn segment_not_present_handler(stack_frame: InterruptStackFrame, error_code: u64)
{
    println!("Exception: segment not present");
    println!("Error code: {:#x}", error_code);
    println!("{:#?}", stack_frame);
    crate::hlt();
}

extern "x86-interrupt" fn stack_segment_fault_handler(stack_frame: InterruptStackFrame, error_code: u64)
{
    println!("Exception: stack segment fault");
    println!("Error code: {:#x}", error_code);
    println!("{:#?}", stack_frame);
    crate::hlt();
}

extern "x86-interrupt" fn alignment_check_handler(stack_frame: InterruptStackFrame, error_code: u64)
{
    println!("Exception: alignment check");
    println!("Error code: {:#x}", error_code);
    println!("{:#?}", stack_frame);
    crate::hlt();
}

extern "x86-interrupt" fn double_fault_handler(stack_frame: InterruptStackFrame, _error_code: u64) -> !
{
    panic!("Exception: double fault\n{:#?}", stack_frame);
}

extern "x86-interrupt" fn general_protection_fault_handler(stack_frame: InterruptStackFrame, error_code: u64,)
{
    println!("Exception: general protection fault");
    println!("Error code: {:#x}", error_code);
    println!("{:#?}", stack_frame);
    crate::hlt();
}

extern "x86-interrupt" fn timer_interrupt_handler(_stack_frame: InterruptStackFrame)
{
    unsafe
    {
        PICS.lock().notify_end_of_interrupt(InterruptIndex::Timer.as_u8());
    }
}

extern "x86-interrupt" fn keyboard_interrupt_handler(_stack_frame: InterruptStackFrame)
{ 
    lazy_static! { static ref KEYBOARD: Mutex<Keyboard<layouts::Us104Key, ScancodeSet1>> = Mutex::new(Keyboard::new(ScancodeSet1::new(), layouts::Us104Key, HandleControl::Ignore)); }

    let mut keyboard = KEYBOARD.lock();
    let mut port = Port::new(0x60);
    let scancode: u8 = unsafe { port.read() };
    if let Ok(Some(key_event)) = keyboard.add_byte(scancode)
    {
        if let Some(key) = keyboard.process_keyevent(key_event)
        {
            match key
            {
                DecodedKey::Unicode(character) => print!("{}", character),
                DecodedKey::RawKey(key) => print!("{:?}", key),
            }
        }
    }

    unsafe 
    {
        PICS.lock().notify_end_of_interrupt(InterruptIndex::Keyboard.as_u8());
    }
}