#include "elf.h"
#include "../fs/vfs.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../drivers/serial/serial.h"

#define ELF_MAX_PAGES 1024

struct elf_page {
    uint64_t vaddr;
    void *phys;
};

static void elf_cleanup(struct elf_page *pages, int count, uint64_t *pml4)
{
    for (int i = 0; i < count; i++)
    {
        if (pages[i].phys)
        {
            if (pages[i].vaddr)
            {
                uint64_t pml4e = pml4[(pages[i].vaddr >> 39) & 0x1FF];
                if (pml4e & PAGE_PRESENT)
                {
                    uint64_t pdpte = ((uint64_t *)(uintptr_t)(pml4e & ~0xFFFULL))[(pages[i].vaddr >> 30) & 0x1FF];
                    if (pdpte & PAGE_PRESENT)
                    {
                        uint64_t pde = ((uint64_t *)(uintptr_t)(pdpte & ~0xFFFULL))[(pages[i].vaddr >> 21) & 0x1FF];
                        if (pde & PAGE_PRESENT)
                        {
                            uint64_t pte = ((uint64_t *)(uintptr_t)(pde & ~0xFFFULL))[(pages[i].vaddr >> 12) & 0x1FF];
                            if (pte & PAGE_PRESENT)
                            {
                                ((uint64_t *)(uintptr_t)(pde & ~0xFFFULL))[(pages[i].vaddr >> 12) & 0x1FF] = 0;
                            }
                        }
                    }
                }
            }
            pmm_free(pages[i].phys);
        }
    }
}

uint64_t elf_load(struct vfs_file *file, uint64_t *pml4)
{
    if (!file || !pml4) return 0;

    struct elf_page pages[ELF_MAX_PAGES];
    int page_count = 0;

    struct elf64_hdr hdr;
    file->offset = 0;
    if (vfs_read(file, sizeof(hdr), &hdr) != (int)sizeof(hdr))
    {
        serial_write("elf: failed to read header\n");
        return 0;
    }

    if (*(uint32_t *)hdr.e_ident != ELF_MAGIC)
    {
        serial_write("elf: bad magic\n");
        return 0;
    }

    if (hdr.e_ident[4] != ELF_64 || hdr.e_ident[5] != ELF_LE)
    {
        serial_write("elf: not 64-bit LE\n");
        return 0;
    }

    if (hdr.e_type != ELF_EXEC)
    {
        serial_write("elf: not executable\n");
        return 0;
    }

    if (hdr.e_phentsize != sizeof(struct elf64_phdr))
    {
        serial_write("elf: unexpected phdr size\n");
        return 0;
    }

    serial_printf("elf: entry=0x%p phnum=%d phoff=0x%p\n",
                  (void*)hdr.e_entry, hdr.e_phnum, (void*)hdr.e_phoff);

    for (uint16_t i = 0; i < hdr.e_phnum; i++)
    {
        struct elf64_phdr phdr;
        file->offset = hdr.e_phoff + i * sizeof(phdr);
        if (vfs_read(file, sizeof(phdr), &phdr) != (int)sizeof(phdr))
        {
            serial_write("elf: failed to read phdr\n");
            elf_cleanup(pages, page_count, pml4);
            return 0;
        }

        if (phdr.p_type != PT_LOAD)
            continue;

        serial_printf("elf: LOAD vaddr=0x%p filesz=0x%p memsz=0x%p offset=0x%p\n",
                      (void*)phdr.p_vaddr, (void*)phdr.p_filesz,
                      (void*)phdr.p_memsz, (void*)phdr.p_offset);

        uint64_t vaddr = phdr.p_vaddr;
        uint64_t page_start = vaddr & ~0xFFFULL;
        uint64_t page_end = (vaddr + phdr.p_memsz + 0xFFF) & ~0xFFFULL;
        uint64_t pages_needed = (page_end - page_start) / PAGE_SIZE;

        if (page_count + (int)pages_needed > ELF_MAX_PAGES)
        {
            serial_write("elf: too many segments\n");
            elf_cleanup(pages, page_count, pml4);
            return 0;
        }

        uint64_t flags = PAGE_USER;
        if (phdr.p_flags & PF_W)
            flags |= PAGE_WRITE;

        for (uint64_t p = 0; p < pages_needed; p++)
        {
            void *phys = pmm_alloc();
            if (!phys)
            {
                serial_write("elf: pmm_alloc failed\n");
                elf_cleanup(pages, page_count, pml4);
                return 0;
            }

            uint64_t page_vaddr = page_start + p * PAGE_SIZE;

            for (uint64_t j = 0; j < PAGE_SIZE; j++)
                ((unsigned char *)phys)[j] = 0;

            if (page_vaddr >= vaddr && page_vaddr < vaddr + phdr.p_filesz)
            {
                uint64_t file_off = phdr.p_offset + (page_vaddr - vaddr);
                uint64_t copy_size = PAGE_SIZE;
                if (page_vaddr + copy_size > vaddr + phdr.p_filesz)
                    copy_size = vaddr + phdr.p_filesz - page_vaddr;
                if (copy_size > 0)
                {
                    file->offset = file_off;
                    vfs_read(file, copy_size, phys);
                }
            }

            pages[page_count].vaddr = page_vaddr;
            pages[page_count].phys = phys;
            page_count++;

            if (vmm_map_page(pml4, page_vaddr, (uint64_t)phys, flags) != 0)
            {
                serial_write("elf: vmm_map_page failed\n");
                elf_cleanup(pages, page_count, pml4);
                return 0;
            }
        }
    }

    return hdr.e_entry;
}
