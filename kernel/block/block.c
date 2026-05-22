#include "block.h"
#include "../drivers/serial/serial.h"

static struct block_device *devices[BLOCK_MAX_DEVS];
static int device_count;

static int part_read(struct block_device *dev, uint64_t sector, uint8_t count, void *buf)
{
    struct partition *part = (struct partition *)dev->private;
    if (!part || !part->parent)
        return -1;
    if (sector + count > part->sector_count)
        return -1;
    return block_read(part->parent, part->start_lba + sector, count, buf);
}

static int part_write(struct block_device *dev, uint64_t sector, uint8_t count, const void *buf)
{
    struct partition *part = (struct partition *)dev->private;
    if (!part || !part->parent)
        return -1;
    if (sector + count > part->sector_count)
        return -1;
    return block_write(part->parent, part->start_lba + sector, count, buf);
}

static struct block_ops part_ops = {
    .read  = part_read,
    .write = part_write,
};

int block_init(void)
{
    device_count = 0;
    for (int i = 0; i < BLOCK_MAX_DEVS; i++)
        devices[i] = NULL;
    return 0;
}

int block_register(struct block_device *dev)
{
    if (!dev || device_count >= BLOCK_MAX_DEVS)
        return -1;
    if (dev->registered)
        return -1;
    if (!dev->ops || !dev->ops->read)
        return -1;
    if (dev->sector_size == 0)
        return -1;

    dev->registered = 1;
    devices[device_count++] = dev;
    serial_printf("block: registered '%s', %llu sectors, %u bytes/sector\n",
                  dev->name, (unsigned long long)dev->sector_count, dev->sector_size);
    return 0;
}

int block_unregister(struct block_device *dev)
{
    if (!dev || !dev->registered)
        return -1;

    for (int i = 0; i < device_count; i++)
    {
        if (devices[i] == dev)
        {
            dev->registered = 0;
            for (int j = i; j < device_count - 1; j++)
                devices[j] = devices[j + 1];
            devices[--device_count] = NULL;
            return 0;
        }
    }
    return -1;
}

int block_read(struct block_device *dev, uint64_t sector, uint8_t count, void *buf)
{
    if (!dev || !dev->registered || !dev->ops || !dev->ops->read)
        return -1;
    if (sector + count > dev->sector_count)
        return -1;
    return dev->ops->read(dev, sector, count, buf);
}

int block_write(struct block_device *dev, uint64_t sector, uint8_t count, const void *buf)
{
    if (!dev || !dev->registered || !dev->ops || !dev->ops->write)
        return -1;
    if (sector + count > dev->sector_count)
        return -1;
    return dev->ops->write(dev, sector, count, buf);
}

struct block_device *block_get(int index)
{
    if (index < 0 || index >= device_count)
        return NULL;
    return devices[index];
}

int block_count(void)
{
    return device_count;
}

struct __attribute__((packed)) mbr_part_entry {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;
    uint8_t  chs_last[3];
    uint32_t lba_first;
    uint32_t sector_count;
};

struct __attribute__((packed)) mbr {
    uint8_t  bootstrap[446];
    struct mbr_part_entry parts[4];
    uint16_t sig;
};

int block_parse_mbr(struct block_device *dev, struct partition *parts, int max_parts)
{
    if (!dev || !parts || max_parts < 1)
        return -1;

    uint8_t mbr_buf[512];
    if (block_read(dev, 0, 1, mbr_buf) != 0)
        return -1;

    struct mbr *m = (struct mbr *)mbr_buf;

    if (m->sig != 0xAA55)
        return -1;

    int count = 0;
    for (int i = 0; i < 4 && count < max_parts; i++)
    {
        if (m->parts[i].type == 0)
            continue;

        parts[count].index       = i + 1;
        parts[count].type        = m->parts[i].type;
        parts[count].bootable    = (m->parts[i].status & 0x80) ? 1 : 0;
        parts[count].start_lba   = m->parts[i].lba_first;
        parts[count].sector_count = m->parts[i].sector_count;
        parts[count].parent      = dev;
        parts[count].dev.private = &parts[count];
        parts[count].dev.ops     = &part_ops;
        count++;
    }

    return count;
}
