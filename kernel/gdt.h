#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define GDT_NULL        0x00
#define GDT_KCODE       0x08
#define GDT_KDATA       0x10
#define GDT_UCODE       0x18
#define GDT_UDATA       0x20
#define GDT_TSS         0x28

struct gdt_entry_t
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr_t
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct tss_t
{
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

void gdt_init(void);
void gdt_set_kstack(uint64_t rsp0);

#endif