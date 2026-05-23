#include "acpi.h"
#include "acpi_common.h"
#include "ports.h"
#include "mm/pmm.h"
#include <stdint.h>
#include <stddef.h>

static volatile uint64_t *ensure_mapped(uint64_t phys)
{
    if (phys >= 0x200000)
    {
        uint64_t aligned = phys & ~0x1FFFFFULL;
        uint64_t entry   = aligned | 0x83;
        volatile uint64_t *pdp = (volatile uint64_t *)0x2000;
        volatile uint64_t *pml4 = (volatile uint64_t *)0x1000;
        int pdp_idx = (int)((aligned >> 30) & 0x1FF);
        int pd_idx  = (int)((aligned >> 21) & 0x1FF);
        if (!(pdp[pdp_idx] & 1))
        {
            uint64_t pd_page = (uint64_t)pmm_alloc();
            volatile uint64_t *pd = (volatile uint64_t *)pd_page;
            for (int i = 0; i < 512; i++)
                pd[i] = 0;
            pdp[pdp_idx] = pd_page | 3;
        }
        volatile uint64_t *pd2 = (volatile uint64_t *)(pdp[pdp_idx] & ~0xFFFULL);
        pd2[pd_idx] = entry;
        __asm__ volatile("invlpg (%0)" : : "r"(phys) : "memory");
    }
    return (volatile uint64_t *)(uint64_t)phys;
}

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

static struct sdt_t *find_sdt(struct rsdp_t *rsdp, const char sig[4])
{
    uint32_t num;
    uint32_t *entries32 = NULL;
    uint64_t *entries64 = NULL;

    if (rsdp->revision >= 2 && rsdp->xsdt_addr)
    {
        uint64_t xsdt_addr = rsdp->xsdt_addr;
        ensure_mapped(xsdt_addr);
        struct sdt_t *xsdt = (struct sdt_t *)(uint64_t)xsdt_addr;
        ensure_mapped(xsdt_addr + xsdt->length);
        num = (xsdt->length - sizeof(struct sdt_t)) / 8;
        entries64 = (uint64_t *)((uint64_t)xsdt_addr + sizeof(struct sdt_t));
    }
    else
    {
        uint32_t rsdt_addr = rsdp->rsdt_addr;
        if (!rsdt_addr) return NULL;
        ensure_mapped(rsdt_addr);
        struct sdt_t *rsdt = (struct sdt_t *)(uint64_t)rsdt_addr;
        ensure_mapped(rsdt_addr + rsdt->length);
        num = (rsdt->length - sizeof(struct sdt_t)) / 4;
        entries32 = (uint32_t *)((uint64_t)rsdt_addr + sizeof(struct sdt_t));
    }

    for (uint32_t i = 0; i < num; i++)
    {
        uint64_t tbl_addr = entries64 ? entries64[i] : entries32[i];
        if (!tbl_addr) continue;
        ensure_mapped(tbl_addr);
        struct sdt_t *tbl = (struct sdt_t *)(uint64_t)tbl_addr;
        if (tbl->signature[0] == sig[0] && tbl->signature[1] == sig[1] &&
            tbl->signature[2] == sig[2] && tbl->signature[3] == sig[3])
            return tbl;
    }
    return NULL;
}

static int parse_s5(const uint8_t *dsdt, uint32_t dsdt_len,
                    uint16_t *slp_typa, uint16_t *slp_typb)
{
    for (uint32_t i = 0; i < dsdt_len - 5; i++)
    {
        if (dsdt[i] != 0x5C) continue;
        if (dsdt[i+1] != 0x5F || dsdt[i+2] != 0x53 ||
            dsdt[i+3] != 0x35 || dsdt[i+4] != 0x5F)
            continue;

        uint32_t pos = i + 5;
        if (dsdt[pos] != 0x08)
            continue;
        pos++;

        if (dsdt[pos] != 0x12)
            continue;
        pos++;

        uint8_t lead = dsdt[pos];
        uint32_t pkg_len = lead & 0x0F;
        uint32_t nbytes = (lead >> 4) & 0x03;
        pos++;
        for (uint32_t j = 0; j < nbytes; j++)
        {
            pkg_len |= (uint32_t)dsdt[pos] << (4 + j * 8);
            pos++;
        }

        int num_elem = dsdt[pos];
        pos++;

        if (num_elem < 2)
            continue;

        if (dsdt[pos] == 0x0A)
        {
            *slp_typa = dsdt[pos+1];
            pos += 2;
        }
        else if (dsdt[pos] == 0x0B)
        {
            *slp_typa = dsdt[pos+1] | (dsdt[pos+2] << 8);
            pos += 3;
        }
        else if (dsdt[pos] == 0x00)
        {
            *slp_typa = 0;
            pos += 1;
        }
        else if (dsdt[pos] == 0x01)
        {
            *slp_typa = 1;
            pos += 1;
        }
        else { continue; }

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

    struct sdt_t *fadt_sdt = find_sdt(rsdp, "FACP");
    if (!fadt_sdt) goto halt;

    struct fadt_t *fadt = (struct fadt_t *)fadt_sdt;
    uint32_t pm1a_cnt = fadt->pm1a_cnt_blk;
    uint32_t pm1b_cnt = fadt->pm1b_cnt_blk;

    uint16_t slp_typa = 0x07;
    uint16_t slp_typb = 0x07;

    uint32_t dsdt_addr = fadt->dsdt;
    if (dsdt_addr)
    {
        ensure_mapped(dsdt_addr);
        struct sdt_t *dsdt_hdr = (struct sdt_t *)(uint64_t)dsdt_addr;
        if (acpi_checksum(dsdt_hdr, dsdt_hdr->length) == 0)
        {
            parse_s5((const uint8_t *)dsdt_hdr, dsdt_hdr->length,
                     &slp_typa, &slp_typb);
        }
    }

    if (pm1a_cnt)
        outw((uint16_t)pm1a_cnt, (slp_typa << 10) | (1 << 13));
    if (pm1b_cnt)
        outw((uint16_t)pm1b_cnt, (slp_typb << 10) | (1 << 13));

halt:
    while (1) __asm__("hlt");
}
