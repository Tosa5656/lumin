pub mod paging
{
    use x86_64::structures::paging::{PageTable, PageTableFlags, PhysFrame};
    use x86_64::registers::control::{Cr0, Cr0Flags, Cr3, Cr3Flags};
    use x86_64::PhysAddr;
    use core::ptr;

    pub const PAGE_SIZE: usize = 4096;
    pub const ENTRIES_PER_TABLE: usize = 512;

    pub struct PageTables
    {
        pub pml4: &'static mut PageTable,
        pub pdpt: &'static mut PageTable,
        pub pd: &'static mut PageTable,
        pub pt: &'static mut PageTable,
    }

    impl PageTables
    {
        pub fn new() -> Self
        {
            let pml4 = unsafe { &mut *(0x1000 as *mut PageTable) };
            let pdpt = unsafe { &mut *(0x2000 as *mut PageTable) };
            let pd = unsafe { &mut *(0x3000 as *mut PageTable) };
            let pt = unsafe { &mut *(0x4000 as *mut PageTable) };

            Self { pml4, pdpt, pd, pt }
        }

        pub fn init_identity_paging(&mut self)
        {
            self.clear_tables();

            let flags = PageTableFlags::PRESENT | PageTableFlags::WRITABLE;

            self.pml4[0].set_addr(PhysAddr::new(0x2000), flags);
            self.pdpt[0].set_addr(PhysAddr::new(0x3000), flags);

            for i in 0..4
            {
                let pd_index = i;
                let pt_addr = 0x4000 + (pd_index * PAGE_SIZE) as u64;
                self.pd[pd_index].set_addr(PhysAddr::new(pt_addr), flags);
            }

            for i in 0..1024
            {
                let phys_addr = (i * PAGE_SIZE) as u64;
                let flags = PageTableFlags::PRESENT | PageTableFlags::WRITABLE;
                self.pt[i].set_addr(PhysAddr::new(phys_addr), flags);
            }
        }

        fn clear_tables(&mut self)
        {
            unsafe
            {
                ptr::write_bytes(self.pml4 as *mut PageTable as *mut u8, 0, PAGE_SIZE);
                ptr::write_bytes(self.pdpt as *mut PageTable as *mut u8, 0, PAGE_SIZE);
                ptr::write_bytes(self.pd as *mut PageTable as *mut u8, 0, PAGE_SIZE);
                ptr::write_bytes(self.pt as *mut PageTable as *mut u8, 0, PAGE_SIZE);
            }
        }
    }

    pub fn init()
    {
        let mut tables = PageTables::new();
        tables.init_identity_paging();

        let pml4_frame = PhysFrame::containing_address(PhysAddr::new(0x1000));
        unsafe
        {
            Cr3::write(pml4_frame, Cr3Flags::empty());
            Cr0::update(|cr0|
            {
                *cr0 |= Cr0Flags::PAGING;
            });
        }
    }
}