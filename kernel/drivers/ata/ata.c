#include "ata.h"
#include "ports.h"
#include "drivers/serial/serial.h"
#include "block/block.h"
#include "kprintf.h"
#include "mm/pmm.h"
#include "drivers/pci/pci.h"
#include "include/initcall.h"
#include <stddef.h>

static int ata_bmdma_probe(struct ata_device *dev);

#define TIMEOUT_TSC 500000000ULL
#define MAX_DEVICES 4

static struct ata_device devices[MAX_DEVICES];
static struct block_device ata_block_devs[MAX_DEVICES];
static struct block_ops   ata_block_ops;
static int                device_count;

static int ata_block_read(struct block_device *bdev, uint64_t sector, uint8_t count, void *buf)
{
    struct ata_device *dev = (struct ata_device *)bdev->private;
    return ata_read_sectors(dev, sector, count, buf);
}

static int ata_block_write(struct block_device *bdev, uint64_t sector, uint8_t count, const void *buf)
{
    struct ata_device *dev = (struct ata_device *)bdev->private;
    return ata_write_sectors(dev, sector, count, buf);
}

static struct block_ops ata_block_ops = {
    .read  = ata_block_read,
    .write = ata_block_write,
};

static uint16_t reg_data(int bus)         { return bus; }
static uint16_t reg_error(int bus)        { return bus + 1; }
static uint16_t reg_features(int bus)     { return bus + 1; }
static uint16_t reg_sector_count(int bus)  { return bus + 2; }
static uint16_t reg_lba_lo(int bus)       { return bus + 3; }
static uint16_t reg_lba_mid(int bus)      { return bus + 4; }
static uint16_t reg_lba_hi(int bus)       { return bus + 5; }
static uint16_t reg_drive(int bus)        { return bus + 6; }
static uint16_t reg_status(int bus)       { return bus + 7; }
static uint16_t reg_cmd(int bus)          { return bus + 7; }

#define STATUS_BSY  0x80
#define STATUS_DRDY 0x40
#define STATUS_DF   0x20
#define STATUS_DRQ  0x08
#define STATUS_ERR  0x01

#define CMD_READ_PIO      0x20
#define CMD_READ_PIO_EXT  0x24
#define CMD_WRITE_PIO     0x30
#define CMD_WRITE_PIO_EXT 0x34
#define CMD_IDENTIFY      0xEC
#define CMD_IDENTIFY_PACKET 0xA1
#define CMD_FLUSH_CACHE   0xE7
#define CMD_FLUSH_CACHE_EXT 0xEA

#define CMD_ATAPI_PACKET  0xA0

static int poll_status(int bus, uint8_t mask, uint8_t val)
{
    uint64_t deadline = rdtsc() + TIMEOUT_TSC;
    while (rdtsc() < deadline)
    {
        uint8_t s = inb(reg_status(bus));
        if ((s & mask) == val)
            return 0;
        if (s & STATUS_ERR)
            return -1;
        __asm__ volatile("pause");
    }
    return -1;
}

static int poll_bsy(int bus)
{
    return poll_status(bus, STATUS_BSY, 0);
}

static int poll_drq(int bus)
{
    return poll_status(bus, STATUS_BSY | STATUS_DRQ, STATUS_DRQ);
}

static int ata_wait(int bus)
{
    inb(reg_status(bus));
    inb(reg_status(bus));
    inb(reg_status(bus));
    inb(reg_status(bus));
    return poll_bsy(bus);
}

static int ata_sel_device(struct ata_device *dev)
{
    int bus = (dev->bus == ATA_BUS_PRIMARY) ? ATA_PRIMARY_BUS : ATA_SECONDARY_BUS;
    int head = (dev->drive == ATA_MASTER) ? 0xA0 : 0xB0;
    outb(reg_drive(bus), head);
    for (volatile int i = 0; i < 50; i++);
    return ata_wait(bus);
}

static void ata_io_wait(int bus)
{
    inb(reg_status(bus));
    inb(reg_status(bus));
    inb(reg_status(bus));
    inb(reg_status(bus));
}

static int ata_read_pio_28(struct ata_device *dev, uint64_t lba, uint8_t count, void *buf)
{
    if (count == 0) return -1;
    int bus = (dev->bus == ATA_BUS_PRIMARY) ? ATA_PRIMARY_BUS : ATA_SECONDARY_BUS;

    if (poll_bsy(bus)) return -1;

    outb(reg_drive(bus), 0xE0 | ((dev->drive == ATA_SLAVE) ? 0x10 : 0) | ((lba >> 24) & 0x0F));
    outb(reg_sector_count(bus), count);
    outb(reg_lba_lo(bus), lba & 0xFF);
    outb(reg_lba_mid(bus), (lba >> 8) & 0xFF);
    outb(reg_lba_hi(bus), (lba >> 16) & 0xFF);
    outb(reg_cmd(bus), CMD_READ_PIO);
    ata_io_wait(bus);

    uint16_t *ptr = (uint16_t *)buf;
    for (int s = 0; s < count; s++)
    {
        if (poll_drq(bus)) return -1;
        ata_io_wait(bus);
        for (int i = 0; i < 256; i++)
            ptr[s * 256 + i] = inw(reg_data(bus));
        if (poll_bsy(bus))
        {
            if (s + 1 < count) return -1;
        }
    }
    return 0;
}

static int ata_read_pio_48(struct ata_device *dev, uint64_t lba, uint8_t count, void *buf)
{
    if (count == 0) return -1;
    int bus = (dev->bus == ATA_BUS_PRIMARY) ? ATA_PRIMARY_BUS : ATA_SECONDARY_BUS;

    if (poll_bsy(bus)) return -1;

    outb(reg_sector_count(bus), (count >> 8) & 0xFF);
    outb(reg_lba_lo(bus), (lba >> 24) & 0xFF);
    outb(reg_lba_mid(bus), (lba >> 32) & 0xFF);
    outb(reg_lba_hi(bus), (lba >> 40) & 0xFF);
    outb(reg_sector_count(bus), count & 0xFF);
    outb(reg_lba_lo(bus), lba & 0xFF);
    outb(reg_lba_mid(bus), (lba >> 8) & 0xFF);
    outb(reg_lba_hi(bus), (lba >> 16) & 0xFF);
    outb(reg_drive(bus), 0x40 | ((dev->drive == ATA_SLAVE) ? 0x10 : 0));
    outb(reg_cmd(bus), CMD_READ_PIO_EXT);
    ata_io_wait(bus);

    uint16_t *ptr = (uint16_t *)buf;
    for (int s = 0; s < count; s++)
    {
        if (poll_drq(bus)) return -1;
        ata_io_wait(bus);
        for (int i = 0; i < 256; i++)
            ptr[s * 256 + i] = inw(reg_data(bus));
        if (poll_bsy(bus))
        {
            if (s + 1 < count) return -1;
        }
    }
    return 0;
}

static int ata_write_pio_28(struct ata_device *dev, uint64_t lba, uint8_t count, const void *buf)
{
    if (count == 0) return -1;
    int bus = (dev->bus == ATA_BUS_PRIMARY) ? ATA_PRIMARY_BUS : ATA_SECONDARY_BUS;

    if (poll_bsy(bus)) return -1;

    outb(reg_drive(bus), 0xE0 | ((dev->drive == ATA_SLAVE) ? 0x10 : 0) | ((lba >> 24) & 0x0F));
    outb(reg_sector_count(bus), count);
    outb(reg_lba_lo(bus), lba & 0xFF);
    outb(reg_lba_mid(bus), (lba >> 8) & 0xFF);
    outb(reg_lba_hi(bus), (lba >> 16) & 0xFF);
    outb(reg_cmd(bus), CMD_WRITE_PIO);
    ata_io_wait(bus);

    const uint16_t *ptr = (const uint16_t *)buf;
    for (int s = 0; s < count; s++)
    {
        if (poll_drq(bus)) return -1;
        for (int i = 0; i < 256; i++)
            outw(reg_data(bus), ptr[s * 256 + i]);
        if (poll_bsy(bus))
        {
            if (s + 1 < count) return -1;
        }
    }
    return 0;
}

static int ata_write_pio_48(struct ata_device *dev, uint64_t lba, uint8_t count, const void *buf)
{
    if (count == 0) return -1;
    int bus = (dev->bus == ATA_BUS_PRIMARY) ? ATA_PRIMARY_BUS : ATA_SECONDARY_BUS;

    if (poll_bsy(bus)) return -1;

    outb(reg_sector_count(bus), (count >> 8) & 0xFF);
    outb(reg_lba_lo(bus), (lba >> 24) & 0xFF);
    outb(reg_lba_mid(bus), (lba >> 32) & 0xFF);
    outb(reg_lba_hi(bus), (lba >> 40) & 0xFF);
    outb(reg_sector_count(bus), count & 0xFF);
    outb(reg_lba_lo(bus), lba & 0xFF);
    outb(reg_lba_mid(bus), (lba >> 8) & 0xFF);
    outb(reg_lba_hi(bus), (lba >> 16) & 0xFF);
    outb(reg_drive(bus), 0x40 | ((dev->drive == ATA_SLAVE) ? 0x10 : 0));
    outb(reg_cmd(bus), CMD_WRITE_PIO_EXT);
    ata_io_wait(bus);

    const uint16_t *ptr = (const uint16_t *)buf;
    for (int s = 0; s < count; s++)
    {
        if (poll_drq(bus)) return -1;
        for (int i = 0; i < 256; i++)
            outw(reg_data(bus), ptr[s * 256 + i]);
        if (poll_bsy(bus))
        {
            if (s + 1 < count) return -1;
        }
    }
    return 0;
}

int ata_identify(struct ata_device *dev)
{
    int bus = (dev->bus == ATA_BUS_PRIMARY) ? ATA_PRIMARY_BUS : ATA_SECONDARY_BUS;

    if (ata_sel_device(dev)) return -1;

    outb(reg_sector_count(bus), 0);
    outb(reg_lba_lo(bus), 0);
    outb(reg_lba_mid(bus), 0);
    outb(reg_lba_hi(bus), 0);
    outb(reg_cmd(bus), CMD_IDENTIFY);
    ata_io_wait(bus);

    uint8_t st = inb(reg_status(bus));
    if (st == 0) return -1;

    if (poll_bsy(bus)) return -1;

    if (inb(reg_lba_mid(bus)) || inb(reg_lba_hi(bus)))
    {
        outb(reg_cmd(bus), CMD_IDENTIFY_PACKET);
        ata_io_wait(bus);
        if (poll_drq(bus)) return -1;
        for (int i = 0; i < 256; i++)
            dev->ident.raw[i] = inw(reg_data(bus));
        dev->type = (inb(reg_lba_mid(bus)) == 0x14 && inb(reg_lba_hi(bus)) == 0xEB) ? ATA_PATAPI : ATA_SATAPI;
        return 0;
    }

    if (poll_drq(bus)) return -1;

    for (int i = 0; i < 256; i++)
        dev->ident.raw[i] = inw(reg_data(bus));

    uint16_t *id = dev->ident.raw;

    int is_atapi = (id[0] & 0x8000) ? 1 : 0;
    if (is_atapi)
    {
        dev->type = ATA_PATAPI;
        dev->lba48 = 0;
        dev->sectors = 0;
    }
    else
    {
        dev->type = ATA_PATA;
        dev->lba48 = (id[83] & 0x0400) ? 1 : 0;
        if (dev->lba48)
            dev->sectors = *(uint64_t *)&id[100];
        else
            dev->sectors = *(uint32_t *)&id[60];
    }

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

    return 0;
}

static int ata_detect(int bus, int drv, struct ata_device *dev)
{
    int base = (bus == ATA_BUS_PRIMARY) ? ATA_PRIMARY_BUS : ATA_SECONDARY_BUS;
    int control = (bus == ATA_BUS_PRIMARY) ? ATA_CONTROL_PRIMARY : ATA_CONTROL_SECONDARY;

    dev->bus = bus;
    dev->drive = drv;
    dev->present = 0;
    dev->type = ATA_NONE;
    dev->lba48 = 0;
    dev->sectors = 0;

    outb(control, 0x02);
    for (volatile int i = 0; i < 50; i++);

    outb(reg_drive(base), (drv == ATA_MASTER) ? 0xA0 : 0xB0);
    for (volatile int i = 0; i < 50; i++);

    uint8_t st = inb(reg_status(base));
    if (st == 0xFF) return -1;

    outb(reg_cmd(base), CMD_IDENTIFY);
    ata_io_wait(base);

    st = inb(reg_status(base));
    if (st == 0) return -1;

    uint8_t lba_mid = inb(reg_lba_mid(base));
    uint8_t lba_hi  = inb(reg_lba_hi(base));

    int patapi_hint = (lba_mid == 0x14 && lba_hi == 0xEB);

    if (patapi_hint)
    {
        dev->present = 1;
        return 0;
    }

    if (lba_mid != 0 || lba_hi != 0)
    {
        dev->present = 1;
        return 0;
    }

    for (volatile int i = 0; i < 50; i++);
    if (poll_bsy(base)) return -1;
    if (poll_drq(base)) return -1;

    dev->present = 1;
    return 0;
}

int ata_init(void)
{
    device_count = 0;
    int idx = 0;
    int sd_count = 0;
    int cdrom_count = 0;
    int nvme_count = 0;

    ata_block_ops.read  = ata_block_read;
    ata_block_ops.write = ata_block_write;

    for (int bus = 0; bus < 2; bus++)
    {
        for (int drv = 0; drv < 2; drv++)
        {
            if (idx >= MAX_DEVICES) break;

            struct ata_device *dev = &devices[idx];
            if (ata_detect(bus, drv, dev) == 0 && dev->present)
            {
                ata_identify(dev);
                dev->bmdma_avail = 0;
                serial_printf("ATA: %s %s detected",
                              (bus == ATA_BUS_PRIMARY) ? "primary" : "secondary",
                              (drv == ATA_MASTER) ? "master" : "slave");
                if (dev->type == ATA_PATA)
                    ata_bmdma_probe(dev);
                if (dev->type != ATA_NONE)
                {
                    const char *typestr = "?";
                    const char *devname = "?";
                    int *counter = NULL;
                    switch (dev->type)
                    {
                        case ATA_PATA:
                            typestr = "PATA";
                            devname = "sd";
                            counter = &sd_count;
                            break;
                        case ATA_PATAPI:
                            typestr = "PATAPI";
                            devname = "cdrom";
                            counter = &cdrom_count;
                            break;
                        case ATA_SATA:
                            typestr = "SATA";
                            devname = "nvme";
                            counter = &nvme_count;
                            break;
                        case ATA_SATAPI:
                            typestr = "SATAPI";
                            devname = "cdrom";
                            counter = &cdrom_count;
                            break;
                        default: break;
                    }
                    serial_printf(" [%s] %s, %llu sectors",
                                  typestr, dev->model,
                                  (unsigned long long)dev->sectors);

                    if (dev->sectors > 0 && counter)
                    {
                        struct block_device *bdev = &ata_block_devs[idx];
                        int n = ksnprintf(bdev->name, sizeof(bdev->name), "%s%d",
                                          devname, (*counter)++);
                        (void)n;
                        bdev->sector_count = dev->sectors;
                        bdev->sector_size  = 512;
                        bdev->ops          = &ata_block_ops;
                        bdev->private      = dev;
                        bdev->registered   = 0;

                        if (block_register(bdev) == 0)
                            serial_printf(" -> registered as %s", bdev->name);
                    }
                }
                serial_write("\n");
                device_count++;
                idx++;
            }
            else
            {
                dev->present = 0;
                idx++;
            }
        }
    }
    return device_count;
}

int ata_device_count(void)
{
    return device_count;
}

struct ata_device *ata_get_device(int index)
{
    if (index < 0 || index >= MAX_DEVICES)
        return NULL;
    if (!devices[index].present)
        return NULL;
    return &devices[index];
}

int ata_read_sectors(struct ata_device *dev, uint64_t lba, uint16_t count, void *buf)
{
    if (!dev || !dev->present || count == 0 || !buf)
        return -1;

    if (count > 255) count = 255;

    uint8_t cnt8 = (uint8_t)count;

    if (dev->lba48 || lba > 0x0FFFFFFFULL)
        return ata_read_pio_48(dev, lba, cnt8, buf);
    else
        return ata_read_pio_28(dev, lba, cnt8, buf);
}

int ata_write_sectors(struct ata_device *dev, uint64_t lba, uint16_t count, const void *buf)
{
    if (!dev || !dev->present || count == 0 || !buf)
        return -1;

    if (count > 255) count = 255;

    uint8_t cnt8 = (uint8_t)count;
    int r;
    if (dev->lba48 || lba > 0x0FFFFFFFULL)
        r = ata_write_pio_48(dev, lba, cnt8, buf);
    else
        r = ata_write_pio_28(dev, lba, cnt8, buf);

    if (r == 0)
        ata_flush(dev);

    return r;
}

int ata_flush(struct ata_device *dev)
{
    int bus = (dev->bus == ATA_BUS_PRIMARY) ? ATA_PRIMARY_BUS : ATA_SECONDARY_BUS;

    if (poll_bsy(bus)) return -1;

    if (dev->lba48)
        outb(reg_cmd(bus), CMD_FLUSH_CACHE_EXT);
    else
        outb(reg_cmd(bus), CMD_FLUSH_CACHE);

    return poll_bsy(bus);
}

int ata_soft_reset(struct ata_device *dev)
{
    int control = (dev->bus == ATA_BUS_PRIMARY) ? ATA_CONTROL_PRIMARY : ATA_CONTROL_SECONDARY;

    outb(control, 0x06);
    for (volatile int i = 0; i < 50; i++);
    outb(control, 0x02);
    for (volatile int i = 0; i < 5000; i++);

    return 0;
}

#define BMDMA_CMD_START 1
#define BMDMA_CMD_STOP  0
#define BMDMA_CMD_READ  0
#define BMDMA_CMD_WRITE 8

#define BMDMA_STAT_ACTIVE   (1 << 0)
#define BMDMA_STAT_ERROR    (1 << 1)
#define BMDMA_STAT_INT      (1 << 2)
#define BMDMA_STAT_DRV0     (1 << 5)
#define BMDMA_STAT_DRV1     (1 << 6)
#define BMDMA_STAT_SIMPLEX  (1 << 7)

#define CMD_READ_DMA      0xC8
#define CMD_READ_DMA_EXT  0x25
#define CMD_WRITE_DMA     0xCA
#define CMD_WRITE_DMA_EXT 0x35

struct prd_entry {
    uint32_t buf_phys;
    uint16_t byte_count;
    uint16_t flags;
};

static struct pci_device *ata_pci_controller;

static int ata_bmdma_probe(struct ata_device *dev)
{
    if (!ata_pci_controller)
    {
        ata_pci_controller = pci_find_device_by_class(0x01, 0x01);
        if (!ata_pci_controller)
            return -1;
    }

    uint16_t bm_base = (uint16_t)(ata_pci_controller->bar[4] & 0xFFFC);
    if (bm_base == 0 || !(ata_pci_controller->prog_if & 0x80))
    {
        serial_printf("ATA: IDE controller has no Bus Master\n");
        return -1;
    }

    void *prdt = pmm_alloc_below(0x100000000ULL);
    if (!prdt) return -1;

    for (int i = 0; i < 128; i++)
        ((uint32_t *)prdt)[i] = 0;

    dev->bmdma_avail  = 1;
    dev->bmdma_base   = bm_base + (dev->bus == ATA_BUS_SECONDARY ? 8 : 0);
    dev->prdt_phys    = prdt;
    dev->prdt_entries = 0;

    serial_printf("ATA: BMDMA at 0x%04x (PRDT at %p)\n",
                  dev->bmdma_base, prdt);
    return 0;
}

static int ata_bmdma_start(struct ata_device *dev, int write, struct prd_entry *prdt, int count)
{
    uint16_t base = dev->bmdma_base;

    outb(base, BMDMA_CMD_STOP);
    outb(base + 2, 0xFF);

    uint8_t cmd = write ? BMDMA_CMD_WRITE : BMDMA_CMD_READ;

    uint32_t prdt_phys = (uint32_t)(uintptr_t)dev->prdt_phys;
    outl(base + 4, prdt_phys);

    struct prd_entry *dma_prdt = (struct prd_entry *)dev->prdt_phys;
    for (int i = 0; i < count; i++)
        dma_prdt[i] = prdt[i];
    if (count > 0)
        dma_prdt[count - 1].flags |= 0x8000;

    outb(base, cmd | BMDMA_CMD_START);
    return 0;
}

static int ata_bmdma_poll(struct ata_device *dev, uint64_t timeout_us)
{
    uint16_t base = dev->bmdma_base;
    uint64_t deadline = rdtsc() + timeout_us * 2000;

    while (rdtsc() < deadline)
    {
        uint8_t stat = inb(base + 2);
        if (!(stat & BMDMA_STAT_ACTIVE))
        {
            if (stat & BMDMA_STAT_ERROR)
                return -1;
            return 0;
        }
        __asm__ volatile("pause");
    }

    outb(base, BMDMA_CMD_STOP);
    return -1;
}

static int ata_bmdma_setup_prdt(struct ata_device *dev, const void *buf, uint64_t total_bytes,
                                 struct prd_entry *prdt, int max_entries)
{
    uint64_t phys = (uint64_t)(uintptr_t)buf;
    int count = 0;

    if ((phys + total_bytes) > 0x100000000ULL)
    {
        serial_printf("ATA: buffer above 4GB, DMA not possible (phys=%p)\n", (void*)phys);
        return -1;
    }

    while (total_bytes > 0 && count < max_entries)
    {
        uint32_t chunk32 = (total_bytes >= 65536) ? 65536 : (uint32_t)(total_bytes & 0xFFFE);
        if (chunk32 == 0) chunk32 = 65536;

        prdt[count].buf_phys    = (uint32_t)(phys & 0xFFFFFFFF);
        prdt[count].byte_count  = (uint16_t)chunk32;
        prdt[count].flags       = 0;

        phys += chunk32;
        total_bytes -= chunk32;
        count++;
    }

    return count;
}

int ata_read_dma(struct ata_device *dev, uint64_t lba, uint16_t count, void *buf)
{
    if (count == 0 || !buf || !dev->bmdma_avail)
        return -1;

    int bus = (dev->bus == ATA_BUS_PRIMARY) ? ATA_PRIMARY_BUS : ATA_SECONDARY_BUS;

    uint64_t total_bytes = (uint64_t)count * 512;
    struct prd_entry prdt[256];
    int prd_count = ata_bmdma_setup_prdt(dev, buf, total_bytes, prdt, 256);
    if (prd_count <= 0) return -1;

    if (poll_bsy(bus)) return -1;

    if (dev->lba48)
    {
        outb(reg_sector_count(bus), (count >> 8) & 0xFF);
        outb(reg_lba_lo(bus), (lba >> 24) & 0xFF);
        outb(reg_lba_mid(bus), (lba >> 32) & 0xFF);
        outb(reg_lba_hi(bus), (lba >> 40) & 0xFF);
        outb(reg_sector_count(bus), count & 0xFF);
        outb(reg_lba_lo(bus), lba & 0xFF);
        outb(reg_lba_mid(bus), (lba >> 8) & 0xFF);
        outb(reg_lba_hi(bus), (lba >> 16) & 0xFF);
        outb(reg_drive(bus), 0x40 | ((dev->drive == ATA_SLAVE) ? 0x10 : 0));
        ata_io_wait(bus);
        if (ata_bmdma_start(dev, 0, prdt, prd_count)) return -1;
        outb(reg_cmd(bus), CMD_READ_DMA_EXT);
    }
    else
    {
        outb(reg_drive(bus), 0xE0 | ((dev->drive == ATA_SLAVE) ? 0x10 : 0) | ((lba >> 24) & 0x0F));
        outb(reg_sector_count(bus), count);
        outb(reg_lba_lo(bus), lba & 0xFF);
        outb(reg_lba_mid(bus), (lba >> 8) & 0xFF);
        outb(reg_lba_hi(bus), (lba >> 16) & 0xFF);
        ata_io_wait(bus);
        if (ata_bmdma_start(dev, 0, prdt, prd_count)) return -1;
        outb(reg_cmd(bus), CMD_READ_DMA);
    }

    ata_io_wait(bus);
    int r = ata_bmdma_poll(dev, 5000000);
    return r;
}

int ata_write_dma(struct ata_device *dev, uint64_t lba, uint16_t count, const void *buf)
{
    if (count == 0 || !buf || !dev->bmdma_avail)
        return -1;

    int bus = (dev->bus == ATA_BUS_PRIMARY) ? ATA_PRIMARY_BUS : ATA_SECONDARY_BUS;

    uint64_t total_bytes = (uint64_t)count * 512;
    struct prd_entry prdt[256];
    int prd_count = ata_bmdma_setup_prdt(dev, buf, total_bytes, prdt, 256);
    if (prd_count <= 0) return -1;

    if (poll_bsy(bus)) return -1;

    if (dev->lba48)
    {
        outb(reg_sector_count(bus), (count >> 8) & 0xFF);
        outb(reg_lba_lo(bus), (lba >> 24) & 0xFF);
        outb(reg_lba_mid(bus), (lba >> 32) & 0xFF);
        outb(reg_lba_hi(bus), (lba >> 40) & 0xFF);
        outb(reg_sector_count(bus), count & 0xFF);
        outb(reg_lba_lo(bus), lba & 0xFF);
        outb(reg_lba_mid(bus), (lba >> 8) & 0xFF);
        outb(reg_lba_hi(bus), (lba >> 16) & 0xFF);
        outb(reg_drive(bus), 0x40 | ((dev->drive == ATA_SLAVE) ? 0x10 : 0));
        ata_io_wait(bus);
        if (ata_bmdma_start(dev, 1, prdt, prd_count)) return -1;
        outb(reg_cmd(bus), CMD_WRITE_DMA_EXT);
    }
    else
    {
        outb(reg_drive(bus), 0xE0 | ((dev->drive == ATA_SLAVE) ? 0x10 : 0) | ((lba >> 24) & 0x0F));
        outb(reg_sector_count(bus), count);
        outb(reg_lba_lo(bus), lba & 0xFF);
        outb(reg_lba_mid(bus), (lba >> 8) & 0xFF);
        outb(reg_lba_hi(bus), (lba >> 16) & 0xFF);
        ata_io_wait(bus);
        if (ata_bmdma_start(dev, 1, prdt, prd_count)) return -1;
        outb(reg_cmd(bus), CMD_WRITE_DMA);
    }

    ata_io_wait(bus);
    int r = ata_bmdma_poll(dev, 5000000);
    if (r == 0)
        ata_flush(dev);
    return r;
}