#![no_std]
#![no_main]
#![feature(abi_x86_interrupt)]
#![feature(slice_ptr_get)]

pub mod vga;
pub mod gdt;
pub mod interrupts;
pub mod multiboot;
pub mod paging;
pub mod allocator;

pub use paging::paging::{PageTables, PAGE_SIZE, ENTRIES_PER_TABLE};
pub use allocator::allocator::{init_frame_allocator, allocate_frame, deallocate_frame, get_frame_allocator_stats};

use core::panic::PanicInfo;
use spin::Mutex;
use lazy_static::lazy_static;
use multiboot::MultibootInfo;
use allocator::allocator::FRAME_ALLOCATOR;
use x86_64::structures::paging::Mapper;


const IS_DEBUG: bool = false;
const LUMIN_VERSION: &str = "0.1";
const LUMIN_LOGO: &str = r" _                        _        
| |     _   _  _ __ ___  (_) _ __  
| |    | | | || '_ ` _ \ | || '_ \ 
| |___ | |_| || | | | | || || | | |
|_____| \__,_||_| |_| |_||_||_| |_|";
                                   


lazy_static! {
    static ref MULTIBOOT_INFO: Mutex<Option<multiboot::MultibootInfo>> = Mutex::new(None);
}

pub fn init()
{
    paging::paging::init();
    init_frame_allocator();
    gdt::init();
    interrupts::init_idt();
    unsafe { interrupts::PICS.lock().initialize(); }
    x86_64::instructions::interrupts::enable();
}

#[unsafe(no_mangle)]
pub extern "C" fn kernel_entry(magic: u64, mb_info_addr: u64) -> !
{
    println!("Welcome to Lumin v{}", LUMIN_VERSION);
    println!("Booting Lumin...");
    
    if magic != multiboot::MULTIBOOT2_BOOTLOADER_MAGIC
    {
        println!("ERROR: Invalid bootloader magic: 0x{:x}", magic);
        hlt();
    }
    
    println!("Parsing multiboot2 info...");
    
    let mb_info_ptr = mb_info_addr as *const u8;
    
    let boot_info = match unsafe {MultibootInfo::load(mb_info_ptr)}
    {
        Ok(info) => info,
        Err(e) =>
        {
            println!("ERROR: Failed to load multiboot info: {}", e);
            hlt();
        }
    };

    *MULTIBOOT_INFO.lock() = Some(boot_info);
    
    if IS_DEBUG == true
    {
        print_multiboot_info();
    }

    println!("{}", LUMIN_LOGO);

    hlt();
}

pub fn hlt() -> !
{
    loop
    {
        x86_64::instructions::hlt();
    }
}

#[panic_handler]
fn panic(info: &PanicInfo) -> !
{
    println!("PANIC: {}", info);
    hlt();
}

pub fn get_memory_map() -> Option<&'static [multiboot::MemoryMapEntry]>
{
    MULTIBOOT_INFO.lock().as_ref().and_then(|info| info.memory_map_entries)
}

pub fn get_total_memory() -> Option<u64>
{
    let info = MULTIBOOT_INFO.lock();
    let info = info.as_ref()?;

    info.memory_map_entries.map(|entries|
    {
        entries.iter().filter(|e| e.typ == 1).map(|e| e.length).sum()
    })
}

pub fn create_page_tables() -> PageTables
{
    PageTables::new()
}

pub fn map_page(virt_addr: x86_64::VirtAddr, phys_frame: x86_64::structures::paging::PhysFrame<x86_64::structures::paging::Size4KiB>)
{
    use x86_64::structures::paging::{OffsetPageTable, Page, PageTableFlags, Size4KiB};
    use x86_64::registers::control::Cr3;

    let page = Page::<Size4KiB>::containing_address(virt_addr);
    let flags = PageTableFlags::PRESENT | PageTableFlags::WRITABLE;

    let (l4_frame, _) = Cr3::read();
    let phys = l4_frame.start_address();
    let virt = x86_64::VirtAddr::new(phys.as_u64());
    let mut mapper = unsafe { OffsetPageTable::new(&mut *virt.as_mut_ptr(), virt) };

    unsafe
    {
        mapper.map_to(page, phys_frame, flags, &mut *FRAME_ALLOCATOR.lock().as_mut().unwrap()).unwrap().flush();
    }
}

pub fn unmap_page(virt_addr: x86_64::VirtAddr)
{
    use x86_64::structures::paging::{OffsetPageTable, Page, Size4KiB};
    use x86_64::registers::control::Cr3;

    let page = Page::<Size4KiB>::containing_address(virt_addr);

    let (l4_frame, _) = Cr3::read();
    let phys = l4_frame.start_address();
    let virt = x86_64::VirtAddr::new(phys.as_u64());
    let mut mapper = unsafe { OffsetPageTable::new(&mut *virt.as_mut_ptr(), virt) };

    mapper.unmap(page).unwrap().1.flush();
}

fn print_multiboot_info()
{
    let info = MULTIBOOT_INFO.lock();
    let info = match info.as_ref()
    {
        Some(i) => i,
        None =>
        {
            println!("ERROR: Multiboot info not available");
            return;
        }
    };
    
    println!("\n=== Multiboot2 Information ===");
    println!("Total size: {} bytes", info.total_size);
    println!();
    
    if let Some(cmdline) = info.cmdline
    {
        println!("[Tag 1] Command line: {}", cmdline);
    }
    
    if let Some(name) = info.boot_loader_name
    {
        println!("[Tag 2] Boot loader name: {}", name);
    }
    
    if !info.modules.is_empty()
    {
        println!("[Tag 3] Modules ({}):", info.modules.len());
        for (i, module) in info.modules.iter().enumerate()
        {
            println!("  Module {}: 0x{:x} - 0x{:x} (size: {} bytes)", 
                i, module.start, module.end, module.end - module.start);
            if let Some(cmdline) = module.cmdline
            {
                println!("    Cmdline: {}", cmdline);
            }
        }
    }
    
    if let Some(mem_lower) = info.mem_lower
    {
        println!("[Tag 4] Basic memory info:");
        println!("  Lower memory: {} KB", mem_lower);
    }
    if let Some(mem_upper) = info.mem_upper
    {
        println!("  Upper memory: {} KB", mem_upper);
    }
    
    if let Some(bootdev) = &info.boot_device
    {
        println!("[Tag 5] Boot device:");
        println!("  BIOS device: 0x{:x}", bootdev.biosdev);
        println!("  Partition: 0x{:x}", bootdev.partition);
        println!("  Sub-partition: 0x{:x}", bootdev.sub_partition);
    }
    
    if let Some(entries) = info.memory_map_entries
    {
        println!("[Tag 6] Memory map ({} entries):", entries.len());
        for (i, entry) in entries.iter().enumerate()
        {
            let typ_str = match entry.typ
            {
                multiboot::MULTIBOOT_MEMORY_AVAILABLE => "Available",
                multiboot::MULTIBOOT_MEMORY_RESERVED => "Reserved",
                multiboot::MULTIBOOT_MEMORY_ACPI_RECLAIMABLE => "ACPI Reclaimable",
                multiboot::MULTIBOOT_MEMORY_NVS => "NVS",
                multiboot::MULTIBOOT_MEMORY_BADRAM => "BadRAM",
                _ => "Unknown",
            };
            let base_addr = entry.base_addr;
            let length = entry.length;
            println!("  Entry {}: 0x{:016x} - 0x{:016x} ({} KB) [{}]", 
                i, 
                base_addr, 
                base_addr + length,
                length / 1024,
                typ_str);
        }
        if let Some(total) = info.get_total_available_memory()
        {
            println!("  Total available memory: {} MB", total / (1024 * 1024));
        }
        if let Some(reserved) = info.get_total_reserved_memory()
        {
            println!("  Total reserved memory: {} MB", reserved / (1024 * 1024));
        }
        if let Some(highest) = info.get_highest_memory_address()
        {
            println!("  Highest memory address: 0x{:016x}", highest);
        }
    }
    
    if let Some(vbe) = &info.vbe_info
    {
        println!("[Tag 7] VBE info:");
        println!("  Mode: 0x{:x}", vbe.mode);
        println!("  Interface: seg=0x{:x}, off=0x{:x}, len={}", 
            vbe.interface_seg, vbe.interface_off, vbe.interface_len);
        if let Some(mode_info) = vbe.mode_info
        {
            let width = mode_info.width;
            let height = mode_info.height;
            let bpp = mode_info.bpp;
            let pitch = mode_info.pitch;
            println!("  Mode info: {}x{} @ {} bpp, pitch={}", 
                width, height, bpp, pitch);
        }
    }
    
    if let Some(fb) = &info.framebuffer
    {
        println!("[Tag 8] Framebuffer:");
        println!("  Address: 0x{:016x}", fb.addr);
        println!("  Resolution: {}x{}", fb.width, fb.height);
        println!("  Pitch: {} bytes", fb.pitch);
        println!("  BPP: {}", fb.bpp);
        let typ_str = match fb.typ
        {
            multiboot::MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED => "Indexed",
            multiboot::MULTIBOOT_FRAMEBUFFER_TYPE_RGB => "RGB",
            multiboot::MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT => "EGA Text",
            _ => "Unknown",
        };
        println!("  Type: {}", typ_str);
        if let Some(color) = &fb.color_info
        {
            println!("  Color info:");
            println!("    Red:   pos={}, mask={}", color.framebuffer_red_field_position, color.framebuffer_red_mask_size);
            println!("    Green: pos={}, mask={}", color.framebuffer_green_field_position, color.framebuffer_green_mask_size);
            println!("    Blue:  pos={}, mask={}", color.framebuffer_blue_field_position, color.framebuffer_blue_mask_size);
        }
    }
    
    if let Some(elf) = &info.elf_sections
    {
        println!("[Tag 9] ELF sections:");
        println!("  Number of sections: {}", elf.num);
        println!("  Entry size: {}", elf.entsize);
        println!("  String table index: {}", elf.shndx);
        if let Some(sections) = elf.sections
        {
            println!("  Sections ({}):", sections.len());
            for (i, section) in sections.iter().take(10).enumerate()
            {
                let sh_addr = section.sh_addr;
                let sh_size = section.sh_size;
                let sh_offset = section.sh_offset;
                println!("    Section {}: addr=0x{:016x}, size={}, offset=0x{:x}", 
                    i, sh_addr, sh_size, sh_offset);
            }
            if sections.len() > 10
            {
                println!("    ... and {} more sections", sections.len() - 10);
            }
        }
    }
    
    if let Some(apm) = &info.apm_table
    {
        println!("[Tag 10] APM table:");
        println!("  Version: 0x{:x}", apm.version);
        println!("  CSEG: 0x{:x}, offset: 0x{:x}", apm.cseg, apm.offset);
        println!("  CSEG16: 0x{:x}, DSEG: 0x{:x}", apm.cseg_16, apm.dseg);
        println!("  Flags: 0x{:x}", apm.flags);
        println!("  Lengths: CSEG={}, CSEG16={}, DSEG={}", 
            apm.cseg_len, apm.cseg_16_len, apm.dseg_len);
    }
    
    if let Some(ptr) = info.efi32_system_table
    {
        println!("[Tag 11] EFI 32-bit system table: 0x{:x}", ptr);
    }
    
    if let Some(ptr) = info.efi64_system_table
    {
        println!("[Tag 12] EFI 64-bit system table: 0x{:016x}", ptr);
    }
    
    if let Some(smbios) = &info.smbios
    {
        println!("[Tag 13] SMBIOS:");
        println!("  Version: {}.{}", smbios.major, smbios.minor);
        if let Some(entries) = smbios.entries
        {
            println!("  Data size: {} bytes", entries.len());
        }
    }
    
    if let Some(addr) = info.acpi_old_rsdp
    {
        println!("[Tag 14] ACPI old RSDP: 0x{:016x}", addr);
    }
    
    if let Some(addr) = info.acpi_new_rsdp
    {
        println!("[Tag 15] ACPI new RSDP: 0x{:016x}", addr);
    }
    
    if let Some(addr) = info.network
    {
        println!("[Tag 16] Network info: 0x{:016x}", addr);
    }
    
    if let Some(efi_mmap) = &info.efi_memory_map
    {
        println!("[Tag 17] EFI memory map:");
        println!("  Descriptor size: {}", efi_mmap.descr_size);
        println!("  Descriptor version: {}", efi_mmap.descr_version);
        if let Some(entries) = efi_mmap.entries
        {
            println!("  Entries ({}):", entries.len());
            for (i, entry) in entries.iter().take(5).enumerate()
            {
                let typ = entry.typ;
                let phys_addr = entry.phys_addr;
                let virt_addr = entry.virt_addr;
                let num_pages = entry.num_pages;
                println!("    Entry {}: type={}, phys=0x{:016x}, virt=0x{:016x}, pages={}", 
                    i, typ, phys_addr, virt_addr, num_pages);
            }
            if entries.len() > 5
            {
                println!("    ... and {} more entries", entries.len() - 5);
            }
        }
    }
    
    if info.efi_boot_services
    {
        println!("[Tag 18] EFI boot services: NOT TERMINATED");
    }
    
    if let Some(handle) = info.efi32_image_handle
    {
        println!("[Tag 19] EFI 32-bit image handle: 0x{:x}", handle);
    }
    
    if let Some(handle) = info.efi64_image_handle
    {
        println!("[Tag 20] EFI 64-bit image handle: 0x{:016x}", handle);
    }
    
    if let Some(addr) = info.load_base_addr
    {
        println!("[Tag 21] Load base address: 0x{:x}", addr);
    }
    
    println!("\n=== End of Multiboot2 Information ===\n");
}