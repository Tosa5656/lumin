#include "lapic.h"
#include "pit.h"
#include "drivers/mmio.h"
#include "ports.h"

#define IA32_APIC_BASE_MSR 0x1B
#define APIC_ENABLE        (1ULL << 11)

#define LAPIC_SVR          0xF0
#define LAPIC_LVT_TIMER    0x320
#define LAPIC_TIMER_DIV    0x3E0
#define LAPIC_TIMER_INIT   0x380
#define LAPIC_TIMER_CURR   0x390
#define LAPIC_EOI          0xB0

#define TIMER_VECTOR 34

static volatile uint32_t *lapic_base;
static volatile uint64_t ticks;
static int               lapic_ok;

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

static inline uint32_t lapic_read(int reg)
{
    return lapic_base[reg / 4];
}

static inline void lapic_write(int reg, uint32_t val)
{
    lapic_base[reg / 4] = val;
}

static int has_apic(void)
{
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(1), "c"(0));
    return (int)((edx >> 9) & 1);
}

static uint64_t calibrate(uint32_t hz)
{
    lapic_write(LAPIC_TIMER_DIV, 0);
    lapic_write(LAPIC_LVT_TIMER, TIMER_VECTOR | (1 << 17));
    lapic_write(LAPIC_TIMER_INIT, 0xFFFFFFFF);

    uint64_t start = pit_get_ticks();
    uint64_t timeout = start + 100;
    while (pit_get_ticks() - start < 5)
    {
        if (pit_get_ticks() > timeout)
            return 0;
        __asm__ volatile("pause");
    }

    uint32_t curr = lapic_read(LAPIC_TIMER_CURR);
    uint32_t elapsed_ticks = 0xFFFFFFFF - curr;
    if (elapsed_ticks == 0)
        return 0;

    uint64_t lapic_hz = (uint64_t)elapsed_ticks * 200;
    return lapic_hz / hz;
}

int lapic_init(uint32_t hz)
{
    lapic_ok = 0;

    if (!has_apic())
        return -1;

    uint64_t apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);
    uint64_t base = apic_base_msr & 0xFFFFFF000ULL;

    if (!(apic_base_msr & APIC_ENABLE))
    {
        wrmsr(IA32_APIC_BASE_MSR, apic_base_msr | APIC_ENABLE);
        base = (rdmsr(IA32_APIC_BASE_MSR) & 0xFFFFFF000ULL);
    }

    mmio_map_2mb(base);

    lapic_base = (volatile uint32_t *)(uint64_t)base;

    lapic_write(LAPIC_SVR, 0xFF | (1 << 8));

    uint64_t init_count = calibrate(hz);
    if (init_count == 0 || init_count > 0xFFFFFFFFULL)
        return -1;

    lapic_write(LAPIC_LVT_TIMER, TIMER_VECTOR | (1 << 17));
    lapic_write(LAPIC_TIMER_DIV, 0);
    lapic_write(LAPIC_TIMER_INIT, (uint32_t)init_count);

    outb(0x21, 0xFD);

    ticks = 0;
    lapic_ok = 1;
    return 0;
}

void lapic_tick(void)
{
    ticks++;
}

uint64_t lapic_get_ticks(void)
{
    return ticks;
}

void lapic_eoi(void)
{
    lapic_write(LAPIC_EOI, 0);
}

uint64_t lapic_get_base(void)
{
    return (uint64_t)(uintptr_t)lapic_base;
}

uint8_t lapic_read_id(void)
{
    if (!lapic_ok) return 0;
    return (uint8_t)(lapic_read(LAPIC_ID) >> 24);
}

void lapic_send_ipi(uint8_t apic_id, uint32_t type, uint8_t vector)
{
    if (!lapic_ok) return;

    while (lapic_read(LAPIC_ICR_LOW) & (1 << 12));

    lapic_write(LAPIC_ICR_HIGH, (uint32_t)apic_id << 24);
    lapic_write(LAPIC_ICR_LOW, type | vector);
}

void lapic_enable(void)
{
    if (!lapic_ok) return;
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | (1 << 8));
}