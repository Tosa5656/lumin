#include "hpet.h"
#include "../mmio.h"
#include "../../ports.h"

#define RSDP_SIG "RSD PTR "

struct __attribute__((packed)) rsdp_t {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_addr;
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t  ext_checksum;
    uint8_t  reserved[3];
};

struct __attribute__((packed)) sdt_t {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
};

struct __attribute__((packed)) acpi_gas_t {
    uint8_t  address_space;
    uint8_t  bit_width;
    uint8_t  bit_offset;
    uint8_t  access_size;
    uint64_t address;
};

struct __attribute__((packed)) hpet_table_t {
    struct sdt_t      header;
    uint32_t          event_timer_block_id;
    struct acpi_gas_t base_address;
    uint16_t          pci_vendor_id;
    uint8_t           sequence;
};

#define HPET_GCAP_ID  0x000
#define HPET_GEN_CONF 0x010
#define HPET_MAIN_CNT 0x0F0
#define HPET_T0_CONF  0x100
#define HPET_T0_COMP  0x108

static volatile uint64_t *hpet_base;
static volatile uint64_t ticks;
static int               hpet_ok;

#define MAPPED_LIMIT 0x200000

static void ensure_mapped(uint64_t addr)
{
    if (addr >= MAPPED_LIMIT)
        mmio_map_2mb(addr);
}

static uint8_t acpi_checksum(const void *data, size_t len)
{
    uint8_t sum = 0;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++)
        sum += p[i];
    return sum;
}

static void *rsdp_find(void)
{
    for (uint32_t addr = 0xE0000; addr < 0xFFFFF; addr += 16)
    {
        const char *p = (const char *)(uint64_t)addr;
        int match = 1;
        for (int i = 0; i < 8; i++)
        {
            if (p[i] != RSDP_SIG[i])
            {
                match = 0;
                break;
            }
        }
        if (match)
        {
            struct rsdp_t *rsdp = (struct rsdp_t *)(uint64_t)addr;
            if (acpi_checksum(rsdp, 20) == 0)
                return rsdp;
        }
    }
    return NULL;
}

int hpet_init(uint32_t hz)
{
    hpet_ok = 0;

    struct rsdp_t *rsdp = rsdp_find();
    if (!rsdp)
        return -1;

    uint32_t rsdt_addr = rsdp->rsdt_addr;
    if (!rsdt_addr)
        return -1;

    ensure_mapped(rsdt_addr);
    struct sdt_t *rsdt = (struct sdt_t *)(uint64_t)rsdt_addr;

    uint32_t num_entries = (rsdt->length - sizeof(struct sdt_t)) / 4;
    uint32_t *entries = (uint32_t *)((uint64_t)rsdt_addr + sizeof(struct sdt_t));

    struct hpet_table_t *hpet_tbl = NULL;
    for (uint32_t i = 0; i < num_entries; i++)
    {
        uint32_t tbl_addr = entries[i];
        ensure_mapped(tbl_addr);
        struct sdt_t *tbl = (struct sdt_t *)(uint64_t)tbl_addr;
        if (tbl->signature[0] == 'H' && tbl->signature[1] == 'P' &&
            tbl->signature[2] == 'E' && tbl->signature[3] == 'T')
        {
            hpet_tbl = (struct hpet_table_t *)(uint64_t)tbl_addr;
            break;
        }
    }

    if (!hpet_tbl)
        return -1;

    if (hpet_tbl->base_address.address_space != 0)
        return -1;

    uint64_t base = hpet_tbl->base_address.address;
    if (!base)
        return -1;

    ensure_mapped(base);

    hpet_base = (volatile uint64_t *)(uint64_t)base;

    uint64_t cap = hpet_base[HPET_GCAP_ID / 8];
    if (cap == 0xFFFFFFFFFFFFFFFFULL || cap == 0)
        return -1;

    int legacy_cap = (int)((cap >> 15) & 1);
    if (!legacy_cap)
        return -1;

    uint64_t fs_per_tick = (cap >> 32) & 0xFFFFFFFFULL;
    if (fs_per_tick == 0)
        return -1;

    uint64_t ticks_per_sec = 1000000000000000ULL / fs_per_tick;
    uint64_t comp = ticks_per_sec / hz;
    if (comp == 0)
        return -1;

    hpet_base[HPET_GEN_CONF / 8] = 1;

    hpet_base[HPET_T0_CONF / 8] = (1 << 0) | (1 << 2) | (1 << 3);

    uint64_t main_cnt = hpet_base[HPET_MAIN_CNT / 8];
    hpet_base[HPET_T0_COMP / 8] = main_cnt + comp;

    ticks = 0;
    hpet_ok = 1;

    outb(0x21, 0xFC);
    return 0;
}

void hpet_tick(void)
{
    if (hpet_ok)
        ticks++;
}

uint64_t hpet_get_ticks(void)
{
    return ticks;
}
