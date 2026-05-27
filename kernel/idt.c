#include "idt.h"
#include "ports.h"
#include "panic.h"
#include "kprintf.h"
#include "drivers/timer/timer.h"
#include "drivers/timer/lapic.h"
#include "drivers/serial/serial.h"
#include "drivers/vga/vga.h"
#include "proc/task.h"

#define IDT_SIZE 256

static struct idt_entry_t idt[IDT_SIZE] __attribute__((aligned(16)));

extern void isr32_handler(void);
extern void isr33_handler(void);
extern void isr34_handler(void);
extern void isr48_handler(void);

extern void exc0_handler(void);
extern void exc1_handler(void);
extern void exc2_handler(void);
extern void exc3_handler(void);
extern void exc4_handler(void);
extern void exc5_handler(void);
extern void exc6_handler(void);
extern void exc7_handler(void);
extern void exc8_handler(void);
extern void exc9_handler(void);
extern void exc10_handler(void);
extern void exc11_handler(void);
extern void exc12_handler(void);
extern void exc13_handler(void);
extern void exc14_handler(void);
extern void exc15_handler(void);
extern void exc16_handler(void);
extern void exc17_handler(void);
extern void exc18_handler(void);
extern void exc19_handler(void);

extern void keyboard_handler(void);

static const char *exc_names[32] = {
    "#DE", "#DB", "NMI", "#BP", "#OF", "#BR", "#UD", "#NM",
    "#DF", "CSEG", "#TS", "#NP", "#SS", "#GP", "#PF", "RSVD",
    "#MF", "#AC", "#MC", "#XM"
};

static void pic_remap(void)
{
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

static void idt_set_entry(int index, void *handler, uint16_t selector, uint8_t attributes)
{
    uint64_t addr = (uint64_t)handler;
    idt[index].offset_low   = addr & 0xFFFF;
    idt[index].offset_mid   = (addr >> 16) & 0xFFFF;
    idt[index].offset_high  = (addr >> 32) & 0xFFFFFFFF;
    idt[index].selector     = selector;
    idt[index].ist          = 0;
    idt[index].attributes   = attributes;
    idt[index].reserved     = 0;
}

void exception_handler(int vec, struct exception_frame *frame)
{
    __asm__ volatile("cli");

    const char *name = (vec >= 0 && vec < 20) ? exc_names[vec] : "???";
    serial_printf("\n=== EXCEPTION %s (%d) ===\n", name, vec);
    serial_printf("RIP: 0x%p  CS: 0x%x  RFLAGS: 0x%x\n",
                  (void*)frame->rip, (unsigned int)frame->cs, (unsigned int)frame->rflags);
    serial_printf("Error code: 0x%x\n", (unsigned int)frame->error_code);

    if (vec == 14)
    {
        uint64_t cr2;
        __asm__("mov %%cr2, %0" : "=r"(cr2));
        serial_printf("CR2 (fault address): 0x%p\n", (void*)cr2);
    }

    serial_printf("RBP: 0x%p\n", (void*)frame->rbp);
    serial_printf("Stack trace:\n");
    uint64_t rbp = frame->rbp;
    for (int i = 0; i < 16 && rbp; i++)
    {
        uint64_t *fp = (uint64_t*)rbp;
        uint64_t ret = fp[1];
        serial_printf("  #%d 0x%p\n", i, (void*)ret);
        rbp = fp[0];
    }

    char msg[64];
    ksnprintf(msg, sizeof(msg), "RIP: 0x%p  Error: 0x%x", (void*)frame->rip, (unsigned int)frame->error_code);

    panic(name, msg);
}

uint64_t irq_handler(int int_no, uint64_t *pushaq_frame)
{
    if (int_no == 32)
    {
        timer_tick_handler();
        timer_eoi();

        return schedule((uint64_t)pushaq_frame);
    }
    else if (int_no == 33)
    {
        keyboard_handler();
        outb(0x20, 0x20);
        return (uint64_t)pushaq_frame;
    }
    else if (int_no == 34)
    {
        timer_tick_handler();
        lapic_eoi();

        return schedule((uint64_t)pushaq_frame);
    }
    else if (int_no >= 40)
    {
        outb(0xA0, 0x20);
    }

    outb(0x20, 0x20);
    return (uint64_t)pushaq_frame;
}

void idt_init(void)
{
    pic_remap();

    idt_set_entry(0,  exc0_handler,  0x08, 0x8E);
    idt_set_entry(1,  exc1_handler,  0x08, 0x8E);
    idt_set_entry(2,  exc2_handler,  0x08, 0x8E);
    idt_set_entry(3,  exc3_handler,  0x08, 0x8E);
    idt_set_entry(4,  exc4_handler,  0x08, 0x8E);
    idt_set_entry(5,  exc5_handler,  0x08, 0x8E);
    idt_set_entry(6,  exc6_handler,  0x08, 0x8E);
    idt_set_entry(7,  exc7_handler,  0x08, 0x8E);
    idt_set_entry(8,  exc8_handler,  0x08, 0x8E);
    idt_set_entry(9,  exc9_handler,  0x08, 0x8E);
    idt_set_entry(10, exc10_handler, 0x08, 0x8E);
    idt_set_entry(11, exc11_handler, 0x08, 0x8E);
    idt_set_entry(12, exc12_handler, 0x08, 0x8E);
    idt_set_entry(13, exc13_handler, 0x08, 0x8E);
    idt_set_entry(14, exc14_handler, 0x08, 0x8E);
    idt_set_entry(15, exc15_handler, 0x08, 0x8E);
    idt_set_entry(16, exc16_handler, 0x08, 0x8E);
    idt_set_entry(17, exc17_handler, 0x08, 0x8E);
    idt_set_entry(18, exc18_handler, 0x08, 0x8E);
    idt_set_entry(19, exc19_handler, 0x08, 0x8E);

    idt_set_entry(32, isr32_handler, 0x08, 0x8E);
    idt_set_entry(33, isr33_handler, 0x08, 0x8E);
    idt_set_entry(34, isr34_handler, 0x08, 0x8E);
    idt_set_entry(48, isr48_handler, 0x08, 0xEE);

    struct idt_ptr_t idt_ptr;
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}
