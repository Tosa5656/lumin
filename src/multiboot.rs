use core::mem::size_of;
use core::ptr;

pub const MULTIBOOT2_BOOTLOADER_MAGIC: u64 = 0x36d76289;
pub const MULTIBOOT2_TAG_ALIGN: usize = 8;
pub const MULTIBOOT_TAG_TYPE_END: u32 = 0;
pub const MULTIBOOT_TAG_TYPE_CMDLINE: u32 = 1;
pub const MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME: u32 = 2;
pub const MULTIBOOT_TAG_TYPE_MODULE: u32 = 3;
pub const MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: u32 = 4;
pub const MULTIBOOT_TAG_TYPE_BOOTDEV: u32 = 5;
pub const MULTIBOOT_TAG_TYPE_MMAP: u32 = 6;
pub const MULTIBOOT_TAG_TYPE_VBE: u32 = 7;
pub const MULTIBOOT_TAG_TYPE_FRAMEBUFFER: u32 = 8;
pub const MULTIBOOT_TAG_TYPE_ELF_SECTIONS: u32 = 9;
pub const MULTIBOOT_TAG_TYPE_APM: u32 = 10;
pub const MULTIBOOT_TAG_TYPE_EFI32: u32 = 11;
pub const MULTIBOOT_TAG_TYPE_EFI64: u32 = 12;
pub const MULTIBOOT_TAG_TYPE_SMBIOS: u32 = 13;
pub const MULTIBOOT_TAG_TYPE_ACPI_OLD: u32 = 14;
pub const MULTIBOOT_TAG_TYPE_ACPI_NEW: u32 = 15;
pub const MULTIBOOT_TAG_TYPE_NETWORK: u32 = 16;
pub const MULTIBOOT_TAG_TYPE_EFI_MMAP: u32 = 17;
pub const MULTIBOOT_TAG_TYPE_EFI_BS: u32 = 18;
pub const MULTIBOOT_TAG_TYPE_EFI32_IH: u32 = 19;
pub const MULTIBOOT_TAG_TYPE_EFI64_IH: u32 = 20;
pub const MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR: u32 = 21;
pub const MULTIBOOT_MEMORY_AVAILABLE: u32 = 1;
pub const MULTIBOOT_MEMORY_RESERVED: u32 = 2;
pub const MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: u32 = 3;
pub const MULTIBOOT_MEMORY_NVS: u32 = 4;
pub const MULTIBOOT_MEMORY_BADRAM: u32 = 5;
pub const MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED: u8 = 0;
pub const MULTIBOOT_FRAMEBUFFER_TYPE_RGB: u8 = 1;
pub const MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT: u8 = 2;

#[repr(C, packed)]
pub struct TagHeader
{
    pub typ: u32,
    pub size: u32,
}

#[repr(C, packed)]
pub struct CmdlineTag
{
    pub header: TagHeader,
}

#[repr(C, packed)]
pub struct BootLoaderNameTag
{
    pub header: TagHeader,
}

#[repr(C, packed)]
pub struct ModuleTag
{
    pub header: TagHeader,
    pub mod_start: u32,
    pub mod_end: u32,
}

#[repr(C, packed)]
pub struct BasicMemoryInfoTag
{
    pub header: TagHeader,
    pub mem_lower: u32,
    pub mem_upper: u32,
}

#[repr(C, packed)]
pub struct BootDeviceTag
{
    pub header: TagHeader,
    pub biosdev: u32,
    pub partition: u32,
    pub sub_partition: u32,
}

#[repr(C, packed)]
pub struct MemoryMapEntry
{
    pub base_addr: u64,
    pub length: u64,
    pub typ: u32,
    pub reserved: u32,
}

#[repr(C, packed)]
pub struct MemoryMapTag
{
    pub header: TagHeader,
    pub entry_size: u32,
    pub entry_version: u32,
}

#[repr(C, packed)]
pub struct VbeInfoTag
{
    pub header: TagHeader,
    pub vbe_mode: u16,
    pub vbe_interface_seg: u16,
    pub vbe_interface_off: u16,
    pub vbe_interface_len: u16,
}

#[repr(C, packed)]
pub struct VbeModeInfoBlock
{
    pub attributes: u16,
    pub window_a: u8,
    pub window_b: u8,
    pub granularity: u16,
    pub window_size: u16,
    pub segment_a: u16,
    pub segment_b: u16,
    pub win_func_ptr: u32,
    pub pitch: u16,
    pub width: u16,
    pub height: u16,
    pub w_char: u8,
    pub y_char: u8,
    pub planes: u8,
    pub bpp: u8,
    pub banks: u8,
    pub memory_model: u8,
    pub bank_size: u8,
    pub image_pages: u8,
    pub reserved0: u8,
    pub red_mask: u8,
    pub red_position: u8,
    pub green_mask: u8,
    pub green_position: u8,
    pub blue_mask: u8,
    pub blue_position: u8,
    pub reserved_mask: u8,
    pub reserved_position: u8,
    pub direct_color_attributes: u8,
    pub framebuffer: u32,
    pub off_screen_mem_off: u32,
    pub off_screen_mem_size: u16,
    pub reserved1: [u8; 206],
}

#[repr(C, packed)]
pub struct FramebufferTag
{
    pub header: TagHeader,
    pub framebuffer_addr: u64,
    pub framebuffer_pitch: u32,
    pub framebuffer_width: u32,
    pub framebuffer_height: u32,
    pub framebuffer_bpp: u8,
    pub framebuffer_type: u8,
    pub reserved: u8,
}

#[repr(C, packed)]
pub struct FramebufferColorInfo
{
    pub framebuffer_red_field_position: u8,
    pub framebuffer_red_mask_size: u8,
    pub framebuffer_green_field_position: u8,
    pub framebuffer_green_mask_size: u8,
    pub framebuffer_blue_field_position: u8,
    pub framebuffer_blue_mask_size: u8,
}

#[repr(C, packed)]
pub struct ElfSectionHeader
{
    pub sh_name: u32,
    pub sh_type: u32,
    pub sh_flags: u64,
    pub sh_addr: u64,
    pub sh_offset: u64,
    pub sh_size: u64,
    pub sh_link: u32,
    pub sh_info: u32,
    pub sh_addralign: u64,
    pub sh_entsize: u64,
}

#[repr(C, packed)]
pub struct ElfSectionsTag
{
    pub header: TagHeader,
    pub num: u32,
    pub entsize: u32,
    pub shndx: u32,
}

#[repr(C, packed)]
pub struct ApmTableTag
{
    pub header: TagHeader,
    pub version: u16,
    pub cseg: u16,
    pub offset: u32,
    pub cseg_16: u16,
    pub dseg: u16,
    pub flags: u16,
    pub cseg_len: u16,
    pub cseg_16_len: u16,
    pub dseg_len: u16,
}

#[repr(C, packed)]
pub struct Efi32Tag
{
    pub header: TagHeader,
    pub pointer: u32,
}

#[repr(C, packed)]
pub struct Efi64Tag
{
    pub header: TagHeader,
    pub pointer: u64,
}

#[repr(C, packed)]
pub struct SmbiosTag
{
    pub header: TagHeader,
    pub major: u8,
    pub minor: u8,
}

#[repr(C, packed)]
pub struct SmbiosEntry
{
    pub type_: u8,
    pub size: u8,
    pub handle: u16,
}

#[repr(C, packed)]
pub struct AcpiOldRsdpTag
{
    pub header: TagHeader,
}

#[repr(C, packed)]
pub struct AcpiNewRsdpTag
{
    pub header: TagHeader,
}

#[repr(C, packed)]
pub struct NetworkTag
{
    pub header: TagHeader,
}

#[repr(C, packed)]
pub struct EfiMemoryMapEntry
{
    pub typ: u32,
    pub phys_addr: u64,
    pub virt_addr: u64,
    pub num_pages: u64,
    pub attribute: u64,
}

#[repr(C, packed)]
pub struct EfiMemoryMapTag
{
    pub header: TagHeader,
    pub descr_size: u32,
    pub descr_version: u32,
}

#[repr(C, packed)]
pub struct EfiBootServicesTag
{
    pub header: TagHeader,
}

#[repr(C, packed)]
pub struct Efi32ImageHandleTag
{
    pub header: TagHeader,
    pub pointer: u32,
}

#[repr(C, packed)]
pub struct Efi64ImageHandleTag
{
    pub header: TagHeader,
    pub pointer: u64,
}

#[repr(C, packed)]
pub struct LoadBaseAddrTag
{
    pub header: TagHeader,
    pub load_base_addr: u32,
}

unsafe fn read_string_from_tag(addr: *const u8, offset: usize, max_len: usize) -> Option<&'static str>
{
    unsafe
    {
        let string_start = addr.add(offset);
        let mut len = 0;
        
        while len < max_len
        {
            if *string_start.add(len) == 0
            {
                break;
            }
            len += 1;
        }
        
        if len == 0
        {
            return None;
        }
        
        core::str::from_utf8(core::slice::from_raw_parts(string_start, len)).ok()
    }
}

pub struct ModuleInfo
{
    pub start: u32,
    pub end: u32,
    pub cmdline: Option<&'static str>,
}

const MAX_MODULES: usize = 32;

pub struct ModulesList
{
    modules: [Option<ModuleInfo>; MAX_MODULES],
    count: usize,
}

impl ModulesList
{
    pub const fn new() -> Self
    {
        Self
        {
            modules: [const { None }; MAX_MODULES],
            count: 0,
        }
    }
    
    pub fn push(&mut self, module: ModuleInfo) -> Result<(), &'static str>
    {
        if self.count >= MAX_MODULES
        {
            return Err("Too many modules");
        }
        self.modules[self.count] = Some(module);
        self.count += 1;
        Ok(())
    }
    
    pub fn len(&self) -> usize
    {
        self.count
    }
    
    pub fn is_empty(&self) -> bool
    {
        self.count == 0
    }
    
    pub fn iter(&self) -> impl Iterator<Item = &ModuleInfo>
    {
        self.modules[..self.count].iter().filter_map(|m| m.as_ref())
    }
    
    pub fn get(&self, index: usize) -> Option<&ModuleInfo>
    {
        if index < self.count
        {
            self.modules[index].as_ref()
        }
        else
        {
            None
        }
    }
}

pub struct BootDeviceInfo
{
    pub biosdev: u32,
    pub partition: u32,
    pub sub_partition: u32,
}

pub struct FramebufferInfo
{
    pub addr: u64,
    pub width: u32,
    pub height: u32,
    pub pitch: u32,
    pub bpp: u8,
    pub typ: u8,
    pub color_info: Option<FramebufferColorInfo>,
}

pub struct VbeInfo
{
    pub mode: u16,
    pub interface_seg: u16,
    pub interface_off: u16,
    pub interface_len: u16,
    pub mode_info: Option<&'static VbeModeInfoBlock>,
}

pub struct ElfSectionsInfo
{
    pub num: u32,
    pub entsize: u32,
    pub shndx: u32,
    pub sections: Option<&'static [ElfSectionHeader]>,
}

pub struct ApmInfo
{
    pub version: u16,
    pub cseg: u16,
    pub offset: u32,
    pub cseg_16: u16,
    pub dseg: u16,
    pub flags: u16,
    pub cseg_len: u16,
    pub cseg_16_len: u16,
    pub dseg_len: u16,
}

pub struct SmbiosInfo
{
    pub major: u8,
    pub minor: u8,
    pub entries: Option<&'static [u8]>,
}

pub struct EfiMemoryMapInfo
{
    pub descr_size: u32,
    pub descr_version: u32,
    pub entries: Option<&'static [EfiMemoryMapEntry]>,
}

pub struct MultibootInfo
{
    pub total_size: u32,
    pub cmdline: Option<&'static str>,
    pub boot_loader_name: Option<&'static str>,
    pub modules: ModulesList,
    pub mem_lower: Option<u32>,
    pub mem_upper: Option<u32>,
    pub boot_device: Option<BootDeviceInfo>,
    pub memory_map_entries: Option<&'static [MemoryMapEntry]>,
    pub vbe_info: Option<VbeInfo>,
    pub framebuffer: Option<FramebufferInfo>,
    pub elf_sections: Option<ElfSectionsInfo>,
    pub apm_table: Option<ApmInfo>,
    pub efi32_system_table: Option<u32>,
    pub efi64_system_table: Option<u64>,
    pub smbios: Option<SmbiosInfo>,
    pub acpi_old_rsdp: Option<usize>,
    pub acpi_new_rsdp: Option<usize>,
    pub network: Option<usize>,
    pub efi_memory_map: Option<EfiMemoryMapInfo>,
    pub efi_boot_services: bool,
    pub efi32_image_handle: Option<u32>,
    pub efi64_image_handle: Option<u64>,
    pub load_base_addr: Option<u32>,
}

impl MultibootInfo
{
    pub unsafe fn load(addr: *const u8) -> Result<Self, &'static str>
    {
        if addr.is_null()
        {
            return Err("Null pointer");
        }
        
        let total_size = unsafe { ptr::read_unaligned(addr as *const u32) };
        
        let mut info = MultibootInfo
        {
            total_size,
            cmdline: None,
            boot_loader_name: None,
            modules: ModulesList::new(),
            mem_lower: None,
            mem_upper: None,
            boot_device: None,
            memory_map_entries: None,
            vbe_info: None,
            framebuffer: None,
            elf_sections: None,
            apm_table: None,
            efi32_system_table: None,
            efi64_system_table: None,
            smbios: None,
            acpi_old_rsdp: None,
            acpi_new_rsdp: None,
            network: None,
            efi_memory_map: None,
            efi_boot_services: false,
            efi32_image_handle: None,
            efi64_image_handle: None,
            load_base_addr: None,
        };
        
        let mut current = unsafe {addr.add(8) as *const TagHeader};
        let end_addr = addr as usize + total_size as usize;
        
        loop
        {
            if current as usize >= end_addr
            {
                break;
            }
            
            let header = unsafe {ptr::read_unaligned(current)};
            
            if header.typ == MULTIBOOT_TAG_TYPE_END
            {
                break;
            }
            
            match header.typ
            {
                MULTIBOOT_TAG_TYPE_CMDLINE =>
                {
                    if let Some(cmdline) = unsafe {read_string_from_tag(current as *const u8, size_of::<TagHeader>(), (header.size as usize) - size_of::<TagHeader>())}
                    {
                        info.cmdline = Some(cmdline);
                    }
                },
                
                MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME =>
                {
                    if let Some(name) = unsafe {read_string_from_tag(current as *const u8, size_of::<TagHeader>(), (header.size as usize) - size_of::<TagHeader>())}
                    {
                        info.boot_loader_name = Some(name);
                    }
                },
                
                MULTIBOOT_TAG_TYPE_MODULE =>
                {
                    let mod_tag = current as *const ModuleTag;
                    let mod_info = unsafe {ptr::read_unaligned(mod_tag)};
                    
                    let cmdline = unsafe {read_string_from_tag(current as *const u8, size_of::<ModuleTag>(), (header.size as usize) - size_of::<ModuleTag>())};
                    
                    let _ = info.modules.push(ModuleInfo
                    {
                        start: mod_info.mod_start,
                        end: mod_info.mod_end,
                        cmdline,
                    });
                },
                
                MULTIBOOT_TAG_TYPE_BASIC_MEMINFO =>
                {
                    let mem_tag = current as *const BasicMemoryInfoTag;
                    let mem_info = unsafe {ptr::read_unaligned(mem_tag)};
                    info.mem_lower = Some(mem_info.mem_lower);
                    info.mem_upper = Some(mem_info.mem_upper);
                },
                
                MULTIBOOT_TAG_TYPE_BOOTDEV =>
                {
                    let bootdev_tag = current as *const BootDeviceTag;
                    let bootdev_info = unsafe {ptr::read_unaligned(bootdev_tag)};
                    info.boot_device = Some(BootDeviceInfo
                    {
                        biosdev: bootdev_info.biosdev,
                        partition: bootdev_info.partition,
                        sub_partition: bootdev_info.sub_partition,
                    });
                },
                
                MULTIBOOT_TAG_TYPE_MMAP =>
                {
                    let mmap_tag = current as *const MemoryMapTag;
                    let mmap_info = unsafe {ptr::read_unaligned(mmap_tag)};
                    
                    let entries_size = header.size as usize - size_of::<MemoryMapTag>();
                    let num_entries = entries_size / mmap_info.entry_size as usize;
                    
                    if num_entries > 0
                    {
                        let first_entry = (unsafe{mmap_tag.add(1)} as *const u8) as *const MemoryMapEntry;
                        info.memory_map_entries = Some(unsafe {core::slice::from_raw_parts(first_entry, num_entries)});
                    }
                },
                
                MULTIBOOT_TAG_TYPE_VBE =>
                {
                    let vbe_tag = current as *const VbeInfoTag;
                    let vbe_info = unsafe {ptr::read_unaligned(vbe_tag)};
                    
                    let mode_info = if header.size as usize > size_of::<VbeInfoTag>()
                    {
                        let mode_info_ptr = (unsafe{vbe_tag.add(1)} as *const u8) as *const VbeModeInfoBlock;
                        Some(unsafe {&*mode_info_ptr})
                    }
                    else
                    {
                        None
                    };
                    
                    info.vbe_info = Some(VbeInfo
                    {
                        mode: vbe_info.vbe_mode,
                        interface_seg: vbe_info.vbe_interface_seg,
                        interface_off: vbe_info.vbe_interface_off,
                        interface_len: vbe_info.vbe_interface_len,
                        mode_info,
                    });
                },
                
                MULTIBOOT_TAG_TYPE_FRAMEBUFFER =>
                {
                    let fb_tag = current as *const FramebufferTag;
                    let fb_info = unsafe {ptr::read_unaligned(fb_tag)};
                    
                    let color_info = if (fb_info.framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED || fb_info.framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) && header.size as usize >= size_of::<FramebufferTag>() + size_of::<FramebufferColorInfo>()
                    {
                        let color_info_ptr = (unsafe{fb_tag.add(1)} as *const u8) as *const FramebufferColorInfo;
                        Some(unsafe {ptr::read_unaligned(color_info_ptr)})
                    }
                    else
                    {
                        None
                    };
                    
                    info.framebuffer = Some(FramebufferInfo
                    {
                        addr: fb_info.framebuffer_addr,
                        width: fb_info.framebuffer_width,
                        height: fb_info.framebuffer_height,
                        pitch: fb_info.framebuffer_pitch,
                        bpp: fb_info.framebuffer_bpp,
                        typ: fb_info.framebuffer_type,
                        color_info,
                    });
                },
                
                MULTIBOOT_TAG_TYPE_ELF_SECTIONS =>
                {
                    let elf_tag = current as *const ElfSectionsTag;
                    let elf_info = unsafe {ptr::read_unaligned(elf_tag)};
                    
                    let entries_size = header.size as usize - size_of::<ElfSectionsTag>();
                    let num_sections = entries_size / elf_info.entsize as usize;
                    
                    let sections = if num_sections > 0
                    {
                        let first_section = (unsafe{elf_tag.add(1)} as *const u8) as *const ElfSectionHeader;
                        Some(unsafe {core::slice::from_raw_parts(first_section, num_sections)})
                    }
                    else
                    {
                        None
                    };
                    
                    info.elf_sections = Some(ElfSectionsInfo
                    {
                        num: elf_info.num,
                        entsize: elf_info.entsize,
                        shndx: elf_info.shndx,
                        sections,
                    });
                },
                
                MULTIBOOT_TAG_TYPE_APM =>
                {
                    let apm_tag = current as *const ApmTableTag;
                    let apm_info = unsafe {ptr::read_unaligned(apm_tag)};
                    info.apm_table = Some(ApmInfo
                    {
                        version: apm_info.version,
                        cseg: apm_info.cseg,
                        offset: apm_info.offset,
                        cseg_16: apm_info.cseg_16,
                        dseg: apm_info.dseg,
                        flags: apm_info.flags,
                        cseg_len: apm_info.cseg_len,
                        cseg_16_len: apm_info.cseg_16_len,
                        dseg_len: apm_info.dseg_len,
                    });
                },
                
                MULTIBOOT_TAG_TYPE_EFI32 =>
                {
                    let efi32_tag = current as *const Efi32Tag;
                    let efi32_info = unsafe {ptr::read_unaligned(efi32_tag)};
                    info.efi32_system_table = Some(efi32_info.pointer);
                },
                
                MULTIBOOT_TAG_TYPE_EFI64 =>
                {
                    let efi64_tag = current as *const Efi64Tag;
                    let efi64_info = unsafe {ptr::read_unaligned(efi64_tag)};
                    info.efi64_system_table = Some(efi64_info.pointer);
                },
                
                MULTIBOOT_TAG_TYPE_SMBIOS =>
                {
                    let smbios_tag = current as *const SmbiosTag;
                    let smbios_info = unsafe {ptr::read_unaligned(smbios_tag)};
                    
                    let data_size = header.size as usize - size_of::<SmbiosTag>();
                    let entries = if data_size > 0
                    {
                        let data_ptr = (unsafe{smbios_tag.add(1)} as *const u8);
                        Some(unsafe {core::slice::from_raw_parts(data_ptr, data_size)})
                    }
                    else
                    {
                        None
                    };
                    
                    info.smbios = Some(SmbiosInfo
                    {
                        major: smbios_info.major,
                        minor: smbios_info.minor,
                        entries,
                    });
                },
                
                MULTIBOOT_TAG_TYPE_ACPI_OLD =>
                {
                    let data_ptr = (unsafe{current.add(1)} as *const u8);
                    info.acpi_old_rsdp = Some(data_ptr as usize);
                },
                
                MULTIBOOT_TAG_TYPE_ACPI_NEW =>
                {
                    let data_ptr = (unsafe{current.add(1)} as *const u8);
                    info.acpi_new_rsdp = Some(data_ptr as usize);
                },
                
                MULTIBOOT_TAG_TYPE_NETWORK =>
                {
                    let data_ptr = (unsafe{current.add(1)} as *const u8);
                    info.network = Some(data_ptr as usize);
                },
                
                MULTIBOOT_TAG_TYPE_EFI_MMAP =>
                {
                    let efi_mmap_tag = current as *const EfiMemoryMapTag;
                    let efi_mmap_info = unsafe {ptr::read_unaligned(efi_mmap_tag)};
                    
                    let entries_size = header.size as usize - size_of::<EfiMemoryMapTag>();
                    let num_entries = entries_size / efi_mmap_info.descr_size as usize;
                    
                    let entries = if num_entries > 0
                    {
                        let first_entry = (unsafe{efi_mmap_tag.add(1)} as *const u8) as *const EfiMemoryMapEntry;
                        Some(unsafe {core::slice::from_raw_parts(first_entry, num_entries)})
                    }
                    else
                    {
                        None
                    };
                    
                    info.efi_memory_map = Some(EfiMemoryMapInfo
                    {
                        descr_size: efi_mmap_info.descr_size,
                        descr_version: efi_mmap_info.descr_version,
                        entries,
                    });
                },
                
                MULTIBOOT_TAG_TYPE_EFI_BS =>
                {
                    info.efi_boot_services = true;
                },
                
                MULTIBOOT_TAG_TYPE_EFI32_IH =>
                {
                    let efi32_ih_tag = current as *const Efi32ImageHandleTag;
                    let efi32_ih_info = unsafe {ptr::read_unaligned(efi32_ih_tag)};
                    info.efi32_image_handle = Some(efi32_ih_info.pointer);
                },
                
                MULTIBOOT_TAG_TYPE_EFI64_IH =>
                {
                    let efi64_ih_tag = current as *const Efi64ImageHandleTag;
                    let efi64_ih_info = unsafe {ptr::read_unaligned(efi64_ih_tag)};
                    info.efi64_image_handle = Some(efi64_ih_info.pointer);
                },
                
                MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR =>
                {
                    let load_base_tag = current as *const LoadBaseAddrTag;
                    let load_base_info = unsafe {ptr::read_unaligned(load_base_tag)};
                    info.load_base_addr = Some(load_base_info.load_base_addr);
                },
                
                _ => {}
            }
            
            let mut next = current as usize + header.size as usize;
            if next % MULTIBOOT2_TAG_ALIGN != 0
            {
                next += MULTIBOOT2_TAG_ALIGN - (next % MULTIBOOT2_TAG_ALIGN);
            }
            
            if next >= end_addr
            {
                break;
            }
            
            current = next as *const TagHeader;
        }
        
        Ok(info)
    }
    
    pub fn get_total_available_memory(&self) -> Option<u64>
    {
        self.memory_map_entries.map(|entries|
        {
            entries.iter().filter(|e| e.typ == MULTIBOOT_MEMORY_AVAILABLE).map(|e| e.length).sum()
        })
    }
    
    pub fn get_total_reserved_memory(&self) -> Option<u64>
    {
        self.memory_map_entries.map(|entries|
        {
            entries.iter().filter(|e| e.typ == MULTIBOOT_MEMORY_RESERVED).map(|e| e.length).sum()
        })
    }
    
    pub fn get_highest_memory_address(&self) -> Option<u64>
    {
        self.memory_map_entries.and_then(|entries|
        {
            entries.iter().map(|e| e.base_addr + e.length).max()
        })
    }
    
    pub unsafe fn get_acpi_old_rsdp_ptr(&self) -> Option<*const u8>
    {
        self.acpi_old_rsdp.map(|addr| addr as *const u8)
    }
    
    pub unsafe fn get_acpi_new_rsdp_ptr(&self) -> Option<*const u8>
    {
        self.acpi_new_rsdp.map(|addr| addr as *const u8)
    }
    
    pub unsafe fn get_network_ptr(&self) -> Option<*const u8>
    {
        self.network.map(|addr| addr as *const u8)
    }
}
