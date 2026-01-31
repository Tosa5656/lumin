pub mod allocator
{
    use x86_64::structures::paging::{PhysFrame, FrameAllocator, Size4KiB};
    use x86_64::PhysAddr;
    use spin::Mutex;
    use crate::paging::paging::PAGE_SIZE;
    use crate::get_memory_map;

    pub struct BitmapFrameAllocator
    {
        bitmap: &'static mut [u64],
        total_frames: usize,
        used_frames: usize,
        next_free_frame: usize,
    }

    impl BitmapFrameAllocator
    {
        pub fn new() -> Self
        {
            let memory_map = get_memory_map();
            if memory_map.is_none()
            {
                panic!("Memory map not available");
            }

            let entries = memory_map.unwrap();

            let mut max_addr = 0u64;
            for entry in entries
            {
                if entry.typ == 1
                {
                    let end_addr = entry.base_addr + entry.length;
                    if end_addr > max_addr
                    {
                        max_addr = end_addr;
                    }
                }
            }

            let total_frames = (max_addr / PAGE_SIZE as u64) as usize;
            let bitmap_size = (total_frames + 63) / 64;

            let bitmap_start = 0x5000;
            let bitmap = unsafe
            {
                core::slice::from_raw_parts_mut(bitmap_start as *mut u64, bitmap_size)
            };

            for byte in bitmap.iter_mut()
            {
                *byte = 0;
            }

            let mut allocator = Self
            {
                bitmap,
                total_frames,
                used_frames: 0,
                next_free_frame: 0,
            };

            allocator.mark_kernel_frames_used();

            allocator
        }

        fn mark_kernel_frames_used(&mut self)
        {
            let kernel_start = 0x1000;
            let kernel_end = 0x5000 + (self.bitmap.len() * 8) as u64;

            let start_frame = (kernel_start / PAGE_SIZE as u64) as usize;
            let end_frame = ((kernel_end + PAGE_SIZE as u64 - 1) / PAGE_SIZE as u64) as usize;

            for frame_idx in start_frame..end_frame.min(self.total_frames)
            {
                self.mark_used(frame_idx);
            }
        }

        fn mark_used(&mut self, frame_idx: usize)
        {
            if frame_idx >= self.total_frames
            {
                return;
            }

            let bitmap_idx = frame_idx / 64;
            let bit_idx = frame_idx % 64;

            self.bitmap[bitmap_idx] |= 1 << bit_idx;
            self.used_frames += 1;
        }

        fn is_free(&self, frame_idx: usize) -> bool
        {
            if frame_idx >= self.total_frames
            {
                return false;
            }

            let bitmap_idx = frame_idx / 64;
            let bit_idx = frame_idx % 64;

            (self.bitmap[bitmap_idx] & (1 << bit_idx)) == 0
        }

        fn mark_free(&mut self, frame_idx: usize)
        {
            if frame_idx >= self.total_frames
            {
                return;
            }

            let bitmap_idx = frame_idx / 64;
            let bit_idx = frame_idx % 64;

            self.bitmap[bitmap_idx] &= !(1 << bit_idx);
            self.used_frames -= 1;
        }

        pub fn allocate_frame(&mut self) -> Option<PhysFrame<Size4KiB>>
        {
            let mut frame_idx = self.next_free_frame;

            for _ in 0..self.total_frames
            {
                if self.is_free(frame_idx)
                {
                    self.mark_used(frame_idx);
                    self.next_free_frame = frame_idx + 1;
                    return Some(PhysFrame::from_start_address(PhysAddr::new(frame_idx as u64 * PAGE_SIZE as u64)).unwrap());
                }
                frame_idx = (frame_idx + 1) % self.total_frames;
            }

            None
        }

        pub fn deallocate_frame(&mut self, frame: PhysFrame<Size4KiB>)
        {
            let frame_idx = (frame.start_address().as_u64() / PAGE_SIZE as u64) as usize;
            self.mark_free(frame_idx);
        }

        pub fn used_frames(&self) -> usize
        {
            self.used_frames
        }

        pub fn total_frames(&self) -> usize
        {
            self.total_frames
        }
    }

    unsafe impl FrameAllocator<Size4KiB> for BitmapFrameAllocator
    {
        fn allocate_frame(&mut self) -> Option<PhysFrame<Size4KiB>>
        {
            self.allocate_frame()
        }
    }

    lazy_static::lazy_static!
    {
        pub static ref FRAME_ALLOCATOR: Mutex<Option<BitmapFrameAllocator>> = Mutex::new(None);
    }

    pub fn init_frame_allocator()
    {
        let allocator = BitmapFrameAllocator::new();
        *FRAME_ALLOCATOR.lock() = Some(allocator);
    }

    pub fn allocate_frame() -> Option<PhysFrame<Size4KiB>>
    {
        FRAME_ALLOCATOR.lock().as_mut().and_then(|allocator| allocator.allocate_frame())
    }

    pub fn deallocate_frame(frame: PhysFrame<Size4KiB>)
    {
        if let Some(allocator) = FRAME_ALLOCATOR.lock().as_mut()
        {
            allocator.deallocate_frame(frame);
        }
    }

    pub fn get_frame_allocator_stats() -> Option<(usize, usize)>
    {
        FRAME_ALLOCATOR.lock().as_ref().map(|allocator| (allocator.used_frames(), allocator.total_frames()))
    }
}