#include "gdt.h"
#include "kprintf.h"
#include "drivers/serial/serial.h"

__attribute__((aligned(16)))
static uint64_t gdt[8];

static struct tss_t tss = {0};

static uint8_t tss_stack[4096] __attribute__((aligned(16)));

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
    gdt[idx] = 0;
    gdt[idx] |= (limit & 0xFFFF);
    gdt[idx] |= (uint64_t)(base & 0xFFFF) << 16;
    gdt[idx] |= (uint64_t)((base >> 16) & 0xFF) << 32;
    gdt[idx] |= (uint64_t)access << 40;
    gdt[idx] |= (uint64_t)(((flags << 4) | ((limit >> 16) & 0xF)) & 0xFF) << 48;
    gdt[idx] |= (uint64_t)((base >> 24) & 0xFF) << 56;
}

static void gdt_set_tss(int idx, uint64_t tss_base, uint32_t tss_limit)
{
    uint64_t low = 0;
    low |= tss_limit & 0xFFFF;
    low |= (tss_base & 0xFFFF) << 16;
    low |= ((tss_base >> 16) & 0xFF) << 32;
    low |= (uint64_t)0x89 << 40;
    low |= (uint64_t)(((tss_limit >> 16) & 0xF)) << 48;
    low |= ((tss_base >> 24) & 0xFF) << 56;

    uint64_t high = (tss_base >> 32) & 0xFFFFFFFF;
    high |= (uint64_t)0 << 32;

    gdt[idx] = low;
    gdt[idx + 1] = high;
}

void gdt_init(void)
{
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0, 0x9A, 0x2);
    gdt_set_entry(2, 0, 0, 0x92, 0x0);
    gdt_set_entry(3, 0, 0, 0xFA, 0x2);
    gdt_set_entry(4, 0, 0, 0xF2, 0x0);

    tss.rsp[0] = (uint64_t)tss_stack + sizeof(tss_stack);
    gdt_set_tss(5, (uint64_t)&tss, sizeof(tss) - 1);

    struct gdt_ptr_t gdt_ptr;
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint64_t)&gdt;
    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));

    __asm__ volatile(
        "pushq %%rax\n"
        "movq %0, %%rax\n"
        "pushq %%rax\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "retfq\n"
        "1:\n"
        "popq %%rax\n"
        :
        : "i"(GDT_KCODE)
        : "rax");

    __asm__ volatile(
        "movw %0, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        :
        : "i"(GDT_KDATA)
        : "ax");

    __asm__ volatile(
        "movw %0, %%ax\n"
        "ltrw %%ax\n"
        :
        : "i"(GDT_TSS)
        : "ax");

    serial_write("GDT: Ring 0/3 + TSS loaded\n");
}

void gdt_set_kstack(uint64_t rsp0)
{
    tss.rsp[0] = rsp0;
}