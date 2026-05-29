#include "smp.h"
#include "../drivers/serial/serial.h"
#include "../drivers/timer/lapic.h"
#include "../drivers/acpi/acpi_common.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../include/spinlock.h"
#include "../include/percpu.h"

#define MAX_CPUS 16

DEFINE_PER_CPU(int, cpu_number);
DEFINE_PER_CPU(uint64_t, cpu_kstack);

static struct {
    uint8_t apic_id;
    int     online;
} cpus[MAX_CPUS];
static int cpu_count = 1;
static int bsp_apic_id;
static spinlock_t smp_lock = SPINLOCK_INIT;

extern volatile int ap_ready[MAX_CPUS];
volatile int ap_ready[MAX_CPUS];

void smp_init(void)
{
    serial_write("SMP: initializing...\n");
}

static void parse_madt(void)
{
    struct rsdp_t *rsdp = rsdp_find();
    if (!rsdp) return;

    uint32_t rsdt_addr = rsdp->rsdt_addr;
    if (!rsdt_addr) return;

    struct sdt_t *rsdt = (struct sdt_t *)(uint64_t)rsdt_addr;
    uint32_t num_entries = (rsdt->length - sizeof(struct sdt_t)) / 4;
    uint32_t *entries = (uint32_t *)((uint64_t)rsdt_addr + sizeof(struct sdt_t));

    for (uint32_t i = 0; i < num_entries; i++)
    {
        uint32_t tbl_addr = entries[i];
        struct sdt_t *tbl = (struct sdt_t *)(uint64_t)tbl_addr;
        if (tbl->signature[0] == 'A' && tbl->signature[1] == 'P' &&
            tbl->signature[2] == 'I' && tbl->signature[3] == 'C')
        {
            uint8_t *data = (uint8_t *)((uint64_t)tbl_addr + sizeof(struct sdt_t));
            uint32_t len = tbl->length - sizeof(struct sdt_t);
            uint32_t off = 0;

            while (off < len)
            {
                uint8_t type = data[off];
                uint8_t entry_len = data[off + 1];
                if (entry_len < 2) break;

                if (type == 0)
                {
                    uint8_t apic_id = data[off + 3];
                    uint32_t flags = *(uint32_t *)(data + off + 4);
                    if (cpu_count < MAX_CPUS && (flags & 1))
                    {
                        cpus[cpu_count].apic_id = apic_id;
                        cpus[cpu_count].online = 0;
                        serial_printf("SMP: found APIC ID %d\n", apic_id);
                        cpu_count++;
                    }
                }
                off += entry_len;
            }
            break;
        }
    }
}

int smp_cpu_count(void)
{
    return cpu_count;
}

void smp_bringup_aps(void)
{
    bsp_apic_id = lapic_read_id();
    serial_printf("SMP: BSP APIC ID = %d\n", bsp_apic_id);

    cpus[0].apic_id = bsp_apic_id;
    cpus[0].online = 1;

    parse_madt();

    serial_printf("SMP: %d CPU(s) total\n", cpu_count);

    if (cpu_count <= 1)
    {
        serial_write("SMP: single processor, skipping AP bringup\n");
        return;
    }

    for (int i = 1; i < cpu_count; i++)
    {
        uint8_t apic_id = cpus[i].apic_id;
        serial_printf("SMP: bringing up APIC ID %d...\n", apic_id);

        for (volatile int d = 0; d < 100000; d++) __asm__("pause");
        lapic_send_ipi(apic_id, IPI_INIT, 0);
        for (volatile int d = 0; d < 1000000; d++) __asm__("pause");

        for (int j = 0; j < 2; j++)
        {
            for (volatile int d = 0; d < 100000; d++) __asm__("pause");
            lapic_send_ipi(apic_id, IPI_STARTUP, 0x08);
        }

        uint64_t timeout = 100000000ULL;
        volatile uint64_t wait = 0;
        while (!ap_ready[i] && wait < timeout)
        {
            wait++;
            __asm__("pause");
        }

        if (ap_ready[i])
        {
            cpus[i].online = 1;
            serial_printf("SMP: APIC ID %d is online\n", apic_id);
        }
        else
        {
            serial_printf("SMP: APIC ID %d failed to come online\n", apic_id);
        }
    }
}
