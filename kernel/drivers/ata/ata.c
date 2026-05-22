#include "ata.h"
#include "../../ports.h"
#include "../serial/serial.h"
#include "../../block/block.h"
#include "../../kprintf.h"
#include <stddef.h>

#define TIMEOUT_NS  10000000
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

static int poll_status(int bus, uint8_t mask, uint8_t val, int timeout_ns)
{
    for (int i = 0; i < timeout_ns; i++)
    {
        uint8_t s = inb(reg_status(bus));
        if ((s & mask) == val)
            return 0;
        if (s & STATUS_ERR)
            return -1;
        for (volatile int j = 0; j < 10; j++);
    }
    return -1;
}

static int poll_bsy(int bus)
{
    return poll_status(bus, STATUS_BSY, 0, TIMEOUT_NS);
}

static int poll_drq(int bus)
{
    return poll_status(bus, STATUS_BSY | STATUS_DRQ, STATUS_DRQ, TIMEOUT_NS);
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

    ata_block_ops.read  = ata_block_read;
    ata_block_ops.write = ata_block_write;

    for (int bus = 0; bus < 2; bus++)
    {
        for (int drv = 0; drv < 2; drv++)
        {
            struct ata_device *dev = &devices[idx];
            if (ata_detect(bus, drv, dev) == 0 && dev->present)
            {
                ata_identify(dev);
                serial_printf("ATA: %s %s detected",
                              (bus == ATA_BUS_PRIMARY) ? "primary" : "secondary",
                              (drv == ATA_MASTER) ? "master" : "slave");
                if (dev->type != ATA_NONE)
                {
                    const char *typestr = "?";
                    switch (dev->type)
                    {
                        case ATA_PATA:   typestr = "PATA"; break;
                        case ATA_PATAPI: typestr = "PATAPI"; break;
                        case ATA_SATA:   typestr = "SATA"; break;
                        case ATA_SATAPI: typestr = "SATAPI"; break;
                        default: break;
                    }
                    serial_printf(" [%s] %s, %llu sectors",
                                  typestr, dev->model,
                                  (unsigned long long)dev->sectors);

                    if (dev->sectors > 0)
                    {
                        struct block_device *bdev = &ata_block_devs[idx];
                        int n = ksnprintf(bdev->name, sizeof(bdev->name), "ata%c%c",
                                          (bus == ATA_BUS_PRIMARY) ? 'a' : 'b',
                                          (drv == ATA_MASTER) ? 'm' : 's');
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
                if (idx >= MAX_DEVICES) break;
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
