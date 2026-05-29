#include "ahci.h"
#include "ports.h"
#include "drivers/serial/serial.h"
#include "block/block.h"
#include "kprintf.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "drivers/pci/pci.h"
#include "include/initcall.h"
#include <stddef.h>

#define AHCI_ABAR_VADDR 0xFFFF8000F8000000ULL
#define AHCI_MAX_PORTS  32

static struct ahci_hba    ahci_hba;
static int                ahci_device_count;
static struct block_device ahci_block_devs[AHCI_MAX_PORTS];
static struct block_ops   ahci_block_ops;

static inline uint32_t reg_read32(volatile void *base, uint32_t off)
{
    return *(volatile uint32_t *)((uintptr_t)base + off);
}

static inline void reg_write32(volatile void *base, uint32_t off, uint32_t v)
{
    *(volatile uint32_t *)((uintptr_t)base + off) = v;
}

static uint64_t ahci_get_abar(struct pci_device *pdev)
{
    uint32_t bar = pdev->bar[5];
    int      mem_type = bar & 0x6;
    if (mem_type == 0x4)
    {
        serial_printf("AHCI: BAR5 is 64-bit\n");
        uint32_t bar_upper = pci_read32(pdev->bus, pdev->slot, pdev->func, 0x28);
        return (uint64_t)bar_upper << 32 | (bar & 0xFFFFFFF0ULL);
    }
    else
    {
        return bar & 0xFFFFFFF0ULL;
    }
}

static volatile void *ahci_map_abar(uint64_t phys)
{
    uint64_t cr3 = vmm_read_cr3();
    if (vmm_map_range((uint64_t *)(uintptr_t)cr3, AHCI_ABAR_VADDR, phys, 8,
                      PAGE_PRESENT | PAGE_WRITE) != 0)
    {
        serial_printf("AHCI: failed to map ABAR phys=%p\n", (void *)phys);
        return NULL;
    }
    return (volatile void *)AHCI_ABAR_VADDR;
}

static volatile void *ahci_port_regs(struct ahci_hba *hba, int port)
{
    return (volatile void *)((uintptr_t)hba->abar + 0x100 + port * 0x80);
}

static uint32_t ahci_port_read(struct ahci_hba *hba, int port, uint32_t off)
{
    volatile void *regs = ahci_port_regs(hba, port);
    return reg_read32(regs, off);
}

static void ahci_port_write(struct ahci_hba *hba, int port, uint32_t off, uint32_t v)
{
    volatile void *regs = ahci_port_regs(hba, port);
    reg_write32(regs, off, v);
}

static void ahci_port_wait_clear(struct ahci_hba *hba, int port, uint32_t off,
                                  uint32_t mask, int timeout_us)
{
    uint64_t deadline = rdtsc() + (uint64_t)timeout_us * 2000;
    while (rdtsc() < deadline)
    {
        if (!(ahci_port_read(hba, port, off) & mask))
            return;
        __asm__ volatile("pause");
    }
}

static int ahci_port_init(struct ahci_hba *hba, struct ahci_device *dev)
{
    int port = dev->port;
    volatile void *regs = ahci_port_regs(hba, port);

    reg_write32(regs, PORT_CMD, ahci_port_read(hba, port, PORT_CMD) & ~(PORT_CMD_ST | PORT_CMD_FRE));

    ahci_port_wait_clear(hba, port, PORT_CMD, PORT_CMD_CR | PORT_CMD_FR, 100000);

    ahci_port_write(hba, port, PORT_CLB,  (uint32_t)((uintptr_t)dev->clb & 0xFFFFFFFF));
    ahci_port_write(hba, port, PORT_CLBU, (uint32_t)(((uintptr_t)dev->clb >> 32) & 0xFFFFFFFF));
    ahci_port_write(hba, port, PORT_FB,   (uint32_t)((uintptr_t)dev->fb & 0xFFFFFFFF));
    ahci_port_write(hba, port, PORT_FBU,  (uint32_t)(((uintptr_t)dev->fb >> 32) & 0xFFFFFFFF));

    ahci_port_write(hba, port, PORT_SERR, SERR_CLEAR);

    reg_write32(regs, PORT_CMD, ahci_port_read(hba, port, PORT_CMD) | PORT_CMD_FRE);
    reg_write32(regs, PORT_CMD, ahci_port_read(hba, port, PORT_CMD) | PORT_CMD_ST);

    uint32_t ssts = ahci_port_read(hba, port, PORT_SSTS);
    uint32_t det  = ssts & SSTS_DET_MASK;
    if (det != SSTS_DET_PRES)
    {
        serial_printf("AHCI: port %d no device (SSTS.DET=%u)\n", port, det);
        dev->present = 0;
        return -1;
    }

    dev->sig = ahci_port_read(hba, port, PORT_SIG);
    serial_printf("AHCI: port %d sig=0x%08X\n", port, dev->sig);

    if (dev->sig == SATA_SIG_ATA || dev->sig == SATA_SIG_ATAPI)
    {
        dev->present = 1;
        return 0;
    }

    serial_printf("AHCI: port %d unsupported sig 0x%08X\n", port, dev->sig);
    dev->present = 0;
    return -1;
}

static int ahci_cmd_issue(struct ahci_hba *hba, struct ahci_device *dev)
{
    int port = dev->port;

    uint64_t deadline = rdtsc() + 2000000000ULL;
    while (rdtsc() < deadline)
    {
        uint32_t tfd = ahci_port_read(hba, port, PORT_TFD);
        if (!(tfd & (TFD_BSY | TFD_DRQ)))
            goto proceed;
        __asm__ volatile("pause");
    }
    serial_printf("AHCI: port %d busy timeout\n", port);
    return -1;

proceed:
    ahci_port_write(hba, port, PORT_CI, 1);

    deadline = rdtsc() + 5000000000ULL;
    while (rdtsc() < deadline)
    {
        if (!(ahci_port_read(hba, port, PORT_CI) & 1))
        {
            uint32_t is = ahci_port_read(hba, port, PORT_IS);
            if (is & 0x01)
            {
                ahci_port_write(hba, port, PORT_IS, is);
                return 0;
            }
            ahci_port_write(hba, port, PORT_IS, is);
            return 0;
        }
        __asm__ volatile("pause");
    }

    serial_printf("AHCI: port %d command timeout\n", port);
    return -1;
}

static int ahci_identify(struct ahci_hba *hba, struct ahci_device *dev)
{
    int port = dev->port;

    void *id_buf = pmm_alloc();
    if (!id_buf) return -1;

    struct ahci_cmd_header *cmdh = &dev->clb[0];
    struct ahci_cmd_table  *ct   = dev->ct;

    cmdh->opts   = CMDH_CFIS_LEN(5) | CMDH_CLR_BUSY | CMDH_PRDTL(1);
    cmdh->opts2  = 0;
    cmdh->ctba   = (uint32_t)((uintptr_t)ct & 0xFFFFFFFF);
    cmdh->ctbau  = (uint32_t)(((uintptr_t)ct >> 32) & 0xFFFFFFFF);

    for (int i = 0; i < 64; i++)
        ct->cfis[i] = 0;

    struct fis_reg_h2d *fis = (struct fis_reg_h2d *)ct->cfis;
    fis->fis_type      = FIS_TYPE_H2D;
    fis->flags         = 0x80;
    fis->command       = ATA_CMD_IDENTIFY;
    fis->device        = 0;
    fis->count_low     = 0;
    fis->count_high    = 0;

    ct->prdt[0].dba      = (uint32_t)((uintptr_t)id_buf & 0xFFFFFFFF);
    ct->prdt[0].dbau     = (uint32_t)(((uintptr_t)id_buf >> 32) & 0xFFFFFFFF);
    ct->prdt[0].reserved = 0;
    ct->prdt[0].dbc      = 0x1FF;

    if (ahci_cmd_issue(hba, dev) != 0)
    {
        pmm_free(id_buf);
        return -1;
    }

    uint16_t *id = (uint16_t *)id_buf;
    dev->lba48 = (id[83] & 0x0400) ? 1 : 0;
    if (dev->lba48)
        dev->sectors = *(uint64_t *)&id[100];
    else
        dev->sectors = *(uint32_t *)&id[60];

    for (int i = 0; i < 20; i++)
    {
        uint16_t w = id[27 + i];
        dev->model[i * 2]     = w >> 8;
        dev->model[i * 2 + 1] = w & 0xFF;
    }
    dev->model[40] = '\0';
    for (int i = 39; i >= 0; i--)
    {
        if (dev->model[i] == ' ') dev->model[i] = '\0';
        else break;
    }

    pmm_free(id_buf);
    return 0;
}

static int ahci_rw(struct ahci_hba *hba, struct ahci_device *dev,
                    uint64_t sector, uint8_t count, void *buf, int write)
{
    if (!dev->present || count == 0 || !buf)
        return -1;

    int      port  = dev->port;
    uint64_t total = (uint64_t)count * 512;

    struct ahci_cmd_header *cmdh = &dev->clb[0];
    struct ahci_cmd_table  *ct   = dev->ct;

    cmdh->opts   = CMDH_CFIS_LEN(5) | CMDH_CLR_BUSY | CMDH_PRDTL(1);
    if (write)
        cmdh->opts |= CMDH_WRITE;
    cmdh->opts2  = 0;
    cmdh->ctba   = (uint32_t)((uintptr_t)ct & 0xFFFFFFFF);
    cmdh->ctbau  = (uint32_t)(((uintptr_t)ct >> 32) & 0xFFFFFFFF);

    for (int i = 0; i < 64; i++)
        ct->cfis[i] = 0;

    struct fis_reg_h2d *fis = (struct fis_reg_h2d *)ct->cfis;
    fis->fis_type    = FIS_TYPE_H2D;
    fis->flags       = 0x80;
    fis->command     = write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    fis->device      = 0x40;
    fis->lba0        = (uint8_t)(sector & 0xFF);
    fis->lba1        = (uint8_t)((sector >> 8) & 0xFF);
    fis->lba2        = (uint8_t)((sector >> 16) & 0xFF);
    fis->lba3        = (uint8_t)((sector >> 24) & 0xFF);
    fis->lba4        = (uint8_t)((sector >> 32) & 0xFF);
    fis->lba5        = (uint8_t)((sector >> 40) & 0xFF);
    fis->count_low   = count;
    fis->count_high  = 0;

    ct->prdt[0].dba      = (uint32_t)((uintptr_t)buf & 0xFFFFFFFF);
    ct->prdt[0].dbau     = (uint32_t)(((uintptr_t)buf >> 32) & 0xFFFFFFFF);
    ct->prdt[0].reserved = 0;
    ct->prdt[0].dbc      = (uint32_t)(total - 1);

    return ahci_cmd_issue(hba, dev);
}

static int ahci_block_read(struct block_device *bdev, uint64_t sector,
                            uint8_t count, void *buf)
{
    struct ahci_device *dev = (struct ahci_device *)bdev->private;
    return ahci_rw(&ahci_hba, dev, sector, count, buf, 0);
}

static int ahci_block_write(struct block_device *bdev, uint64_t sector,
                             uint8_t count, const void *buf)
{
    struct ahci_device *dev = (struct ahci_device *)bdev->private;
    return ahci_rw(&ahci_hba, dev, sector, count, (void *)buf, 1);
}

static struct block_ops ahci_block_ops = {
    .read  = ahci_block_read,
    .write = ahci_block_write,
};

static void ahci_init_device(struct ahci_hba *hba, struct ahci_device *dev)
{
    int port = dev->port;

    dev->clb = (struct ahci_cmd_header *)pmm_alloc();
    if (!dev->clb) goto fail;
    for (int i = 0; i < 128; i++)
        ((uint32_t *)dev->clb)[i] = 0;

    dev->fb = pmm_alloc();
    if (!dev->fb) goto fail_clb;
    for (int i = 0; i < 1024; i++)
        ((uint32_t *)dev->fb)[i] = 0;

    dev->ct = (struct ahci_cmd_table *)pmm_alloc();
    if (!dev->ct) goto fail_fb;
    for (int i = 0; i < 1024; i++)
        ((uint32_t *)dev->ct)[i] = 0;

    if (ahci_port_init(hba, dev) != 0 || !dev->present)
        goto fail_ct;

    if (dev->sig == SATA_SIG_ATAPI)
    {
        serial_printf("AHCI: port %d ATAPI not supported\n", port);
        goto fail_ct;
    }

    if (ahci_identify(hba, dev) != 0)
    {
        serial_printf("AHCI: port %d identify failed\n", port);
        goto fail_ct;
    }

    serial_printf("AHCI: port %d %s, LBA%c, %llu sectors\n",
                  port, dev->model,
                  dev->lba48 ? '8' : '4',
                  (unsigned long long)dev->sectors);

    struct block_device *bdev = &ahci_block_devs[ahci_device_count];
    int n = ksnprintf(bdev->name, sizeof(bdev->name),
                      "sata%d", ahci_device_count);
    (void)n;
    bdev->sector_count = dev->sectors;
    bdev->sector_size  = 512;
    bdev->ops         = &ahci_block_ops;
    bdev->private     = dev;
    bdev->registered  = 0;

    if (block_register(bdev) == 0)
    {
        serial_printf("AHCI: port %d registered as %s\n", port, bdev->name);
        dev->present = 1;
        ahci_device_count++;
        return;
    }

    serial_printf("AHCI: port %d block registration failed\n", port);

fail_ct:
    pmm_free(dev->ct);
    dev->ct = NULL;
fail_fb:
    pmm_free(dev->fb);
    dev->fb = NULL;
fail_clb:
    pmm_free(dev->clb);
    dev->clb = NULL;
fail:
    dev->present = 0;
}

int ahci_init(void)
{
    struct pci_device *ahci_pdev = NULL;
    for (int i = 0; i < pci_device_count(); i++)
    {
        struct pci_device *pdev = pci_get_device(i);
        if (pdev && pdev->class_code == 0x01 &&
            pdev->subclass == 0x06 && pdev->prog_if == 0x01)
        {
            ahci_pdev = pdev;
            break;
        }
    }

    if (!ahci_pdev)
    {
        serial_printf("AHCI: no controller found\n");
        return -1;
    }

    serial_printf("AHCI: controller at %02x:%02x.%d vendor=%04x device=%04x\n",
                  ahci_pdev->bus, ahci_pdev->slot, ahci_pdev->func,
                  ahci_pdev->vendor, ahci_pdev->device_id);

    uint64_t abar_phys = ahci_get_abar(ahci_pdev);
    serial_printf("AHCI: ABAR at physical %p\n", (void *)abar_phys);

    volatile void *abar = ahci_map_abar(abar_phys);
    if (!abar)
        return -1;

    ahci_hba.abar  = abar;
    ahci_hba.cap   = reg_read32(abar, HBA_CAP);
    ahci_hba.cap2  = reg_read32(abar, HBA_CAP2);
    ahci_hba.pi    = reg_read32(abar, HBA_PI);

    int n_ports = (ahci_hba.cap & CAP_NP_MASK) + 1;
    int n_slots = ((ahci_hba.cap & CAP_NCS_MASK) >> 8) + 1;
    ahci_hba.n_ports = n_ports;
    ahci_hba.n_slots = n_slots;

    serial_printf("AHCI: CAP=0x%08X CAP2=0x%08X PI=0x%08X ports=%d slots=%d\n",
                  ahci_hba.cap, ahci_hba.cap2, ahci_hba.pi, n_ports, n_slots);

    reg_write32(abar, HBA_GHC, GHC_AE);
    reg_read32(abar, HBA_GHC);

    for (int port = 0; port < n_ports && port < AHCI_MAX_PORTS; port++)
    {
        if (!(ahci_hba.pi & (1U << port)))
            continue;

        struct ahci_device *dev = &ahci_hba.devices[port];
        dev->present = 0;
        dev->port    = port;
        dev->clb     = NULL;
        dev->fb      = NULL;
        dev->ct      = NULL;

        ahci_init_device(&ahci_hba, dev);
    }

    serial_printf("AHCI: initialized, %d device(s)\n", ahci_device_count);
    return 0;
}

static int ahci_initcall(void)
{
    return ahci_init();
}
device_initcall(ahci_initcall);
