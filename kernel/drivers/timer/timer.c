#include "timer.h"
#include "pit.h"
#include "hpet.h"
#include "lapic.h"
#include "../../ports.h"

static volatile uint64_t ticks;
static enum timer_type   active = TIMER_NONE;
static uint32_t          freq_hz;

void timer_init(uint32_t hz)
{
    freq_hz = hz;
    ticks = 0;

    pit_init(hz);

    if (hpet_init(hz) == 0)
    {
        active = TIMER_HPET;
        return;
    }

    if (lapic_init(hz) == 0)
    {
        active = TIMER_LAPIC;
        return;
    }

    outb(0x21, 0xFC);
    active = TIMER_PIT;
}

uint64_t timer_get_ticks(void)
{
    return ticks;
}

void timer_sleep(uint32_t ms)
{
    if (!freq_hz) return;
    uint64_t target = ticks + ((uint64_t)freq_hz * ms / 1000);
    while (ticks < target)
        __asm__ volatile("hlt");
}

void timer_tick_handler(void)
{
    ticks++;
}

enum timer_type timer_get_type(void)
{
    return active;
}

void timer_eoi(void)
{
    if (active == TIMER_LAPIC)
        lapic_eoi();
    else
        outb(0x20, 0x20);
}