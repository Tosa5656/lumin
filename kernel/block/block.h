#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stddef.h>

#define BLOCK_NAME_MAX  16
#define BLOCK_MAX_DEVS  16
#define BLOCK_MAX_PARTS 16

struct block_device;

struct block_ops
{
    int (*read)(struct block_device *dev, uint64_t sector, uint8_t count, void *buf);
    int (*write)(struct block_device *dev, uint64_t sector, uint8_t count, const void *buf);
};

struct block_device
{
    char               name[BLOCK_NAME_MAX];
    uint64_t           sector_count;
    uint32_t           sector_size;
    struct block_ops  *ops;
    void              *private;
    int                registered;
};

struct partition
{
    int               index;
    uint8_t           type;
    int               bootable;
    uint64_t          start_lba;
    uint64_t          sector_count;
    struct block_device *parent;
    struct block_device dev;
};

int  block_init(void);
int  block_register(struct block_device *dev);
int  block_unregister(struct block_device *dev);
int  block_read(struct block_device *dev, uint64_t sector, uint8_t count, void *buf);
int  block_write(struct block_device *dev, uint64_t sector, uint8_t count, const void *buf);
struct block_device *block_get(int index);
int  block_count(void);

int  block_parse_mbr(struct block_device *dev, struct partition *parts, int max_parts);
int  block_register_partitions(struct block_device *dev);
int  block_partition_count(void);
struct block_device *block_partition_get(int index);

#endif