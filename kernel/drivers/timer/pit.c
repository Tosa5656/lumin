#include "pit.h"
#include "ports.h"

#define PIT_DATA0    0x40
#define PIT_CMD      0x43
#define PIT_BASE_FREQ 1193182

static volatile uint64_t ticks;

void pit_init(uint32_t hz)
{
    uint32_t divisor = PIT_BASE_FREQ / hz;
    outb(PIT_CMD, 0x36);
    outb(PIT_DATA0, divisor & 0xFF);
    outb(PIT_DATA0, (divisor >> 8) & 0xFF);
    ticks = 0;
}

void pit_stop(void)
{
    outb(PIT_CMD, 0x30);
    outb(PIT_DATA0, 0);
    outb(PIT_DATA0, 0);
}

void pit_tick(void)
{
    ticks++;
}

uint64_t pit_get_ticks(void)
{
    return ticks;
}