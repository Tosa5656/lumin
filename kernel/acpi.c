#include "acpi.h"
#include "ports.h"
#include <stdint.h>
#include <stddef.h>

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

struct __attribute__((packed)) fadt_t {
    struct sdt_t h;
    uint32_t     firmware_ctrl;
    uint32_t     dsdt;
    uint8_t      reserved1;
    uint8_t      preferred_pm_profile;
    uint16_t     sci_int;
    uint32_t     smi_cmd;
    uint8_t      acpi_enable;
    uint8_t      acpi_disable;
    uint8_t      s4bios_req;
    uint8_t      pstate_cnt;
    uint32_t     pm1a_evt_blk;
    uint32_t     pm1b_evt_blk;
    uint32_t     pm1a_cnt_blk;
    uint32_t     pm1b_cnt_blk;
    uint32_t     pm2_cnt_blk;
    uint32_t     pm_tmr_blk;
    uint32_t     gpe0_blk;
    uint32_t     gpe1_blk;
    uint8_t      pm1_evt_len;
    uint8_t      pm1_cnt_len;
    uint8_t      pm2_cnt_len;
    uint8_t      pm_tmr_len;
    uint8_t      gpe0_len;
    uint8_t      gpe1_len;
    uint8_t      gpe1_base;
    uint8_t      cst_cnt;
    uint16_t     plvl2_lat;
    uint16_t     plvl3_lat;
    uint16_t     flush_size;
    uint16_t     flush_stride;
    uint8_t      duty_offset;
    uint8_t      duty_width;
    uint8_t      day_alrm;
    uint8_t      mon_alrm;
    uint8_t      century;
    uint16_t     iapc_boot_arch;
    uint8_t      reserved2;
    uint32_t     flags;
    uint8_t      reset_reg[12];
    uint8_t      reset_value;
};

static uint8_t acpi_checksum(const void *data, size_t len)
{
    uint8_t sum = 0;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++)
        sum += p[i];
    return sum;
}

static struct rsdp_t *rsdp_find(void)
{
    for (uint32_t addr = 0xE0000; addr < 0xFFFFF; addr += 16)
    {
        const char *p = (const char *)(uint64_t)addr;
        int match = 1;
        for (int i = 0; i < 8; i++)
        {
            if (p[i] != RSDP_SIG[i]) { match = 0; break; }
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

static struct fadt_t *fadt_find(struct rsdp_t *rsdp)
{
    uint32_t rsdt_addr = rsdp->rsdt_addr;
    struct sdt_t *rsdt = (struct sdt_t *)(uint64_t)rsdt_addr;
    uint32_t *entries = (uint32_t *)((uint64_t)rsdt_addr + sizeof(struct sdt_t));
    uint32_t num = (rsdt->length - sizeof(struct sdt_t)) / 4;

    for (uint32_t i = 0; i < num; i++)
    {
        struct sdt_t *tbl = (struct sdt_t *)(uint64_t)entries[i];
        if (tbl->signature[0] == 'F' && tbl->signature[1] == 'A' &&
            tbl->signature[2] == 'C' && tbl->signature[3] == 'P')
            return (struct fadt_t *)tbl;
    }
    return NULL;
}

// Ищет \_S5 в DSDT и достаёт SLP_TYPa, SLP_TYPb
// AML: Name(_S5_, Package() { SLP_TYPa, SLP_TYPb, ... })
static int parse_s5(const uint8_t *dsdt, uint32_t dsdt_len,
                    uint16_t *slp_typa, uint16_t *slp_typb)
{
    // Ищем \_S5_ : 0x5C 0x5F 0x53 0x35 0x5F
    for (uint32_t i = 0; i < dsdt_len - 5; i++)
    {
        if (dsdt[i] != 0x5C) continue;
        if (dsdt[i+1] != 0x5F || dsdt[i+2] != 0x53 ||
            dsdt[i+3] != 0x35 || dsdt[i+4] != 0x5F)
            continue;

        // Дальше идёт NameOp (0x08) + value
        uint32_t pos = i + 5;
        if (dsdt[pos] != 0x08)
            continue;
        pos++;

        // PackageOp (0x12)
        if (dsdt[pos] != 0x12)
            continue;
        pos++;

        // PkgLength (может быть 1-4 байта)
        uint32_t pkg_len = dsdt[pos] & 0x3F;
        if (dsdt[pos] & 0x80)
        {
            int nbytes = dsdt[pos] & 0x0F;
            if (nbytes == 0) { pos += 2; }
            else { pos += nbytes + 1; }
        }
        else
        {
            pos++;
        }

        // NumElements (1 байт после PkgLength)
        int num_elem = dsdt[pos];
        pos++;

        if (num_elem < 2)
            continue;

        // SLP_TYPa
        if (dsdt[pos] == 0x0A)        // ByteConst
        {
            *slp_typa = dsdt[pos+1];
            pos += 2;
        }
        else if (dsdt[pos] == 0x0B)   // WordConst
        {
            *slp_typa = dsdt[pos+1] | (dsdt[pos+2] << 8);
            pos += 3;
        }
        else if (dsdt[pos] == 0x00)   // ZeroOp
        {
            *slp_typa = 0;
            pos += 1;
        }
        else if (dsdt[pos] == 0x01)   // OneOp
        {
            *slp_typa = 1;
            pos += 1;
        }
        else { continue; }

        // SLP_TYPb
        if (dsdt[pos] == 0x0A)
        {
            *slp_typb = dsdt[pos+1];
        }
        else if (dsdt[pos] == 0x0B)
        {
            *slp_typb = dsdt[pos+1] | (dsdt[pos+2] << 8);
        }
        else if (dsdt[pos] == 0x00)
        {
            *slp_typb = 0;
        }
        else if (dsdt[pos] == 0x01)
        {
            *slp_typb = 1;
        }
        else { continue; }

        return 0;
    }
    return -1;
}

void acpi_shutdown(void)
{
    __asm__ volatile("cli");

    struct rsdp_t *rsdp = rsdp_find();
    if (!rsdp) goto halt;

    struct fadt_t *fadt = fadt_find(rsdp);
    if (!fadt) goto halt;

    uint32_t pm1a_cnt = fadt->pm1a_cnt_blk;
    uint32_t pm1b_cnt = fadt->pm1b_cnt_blk;

    // Парсим SLP_TYP из DSDT
    uint16_t slp_typa = 0x07;
    uint16_t slp_typb = 0x07;

    struct sdt_t *dsdt_hdr = (struct sdt_t *)(uint64_t)fadt->dsdt;
    if (dsdt_hdr)
    {
        const uint8_t *dsdt_data = (const uint8_t *)dsdt_hdr;
        uint32_t dsdt_len = dsdt_hdr->length;
        parse_s5(dsdt_data, dsdt_len, &slp_typa, &slp_typb);
    }

    // SLP_EN (bit 13) | SLP_TYPx (bits 10-12)
    outw((uint16_t)pm1a_cnt, (slp_typa << 10) | (1 << 13));
    if (pm1b_cnt)
        outw((uint16_t)pm1b_cnt, (slp_typb << 10) | (1 << 13));

halt:
    while (1) __asm__("hlt");
}
