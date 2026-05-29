#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_PRIMARY_BUS    0x1F0
#define ATA_SECONDARY_BUS  0x170
#define ATA_CONTROL_PRIMARY   0x3F6
#define ATA_CONTROL_SECONDARY 0x376

#define ATA_MASTER  0
#define ATA_SLAVE   1

enum ata_bus { ATA_BUS_PRIMARY = 0, ATA_BUS_SECONDARY };

enum ata_drive_type
{
    ATA_NONE,
    ATA_PATA,
    ATA_PATAPI,
    ATA_SATA,
    ATA_SATAPI,
    ATA_UNKNOWN
};

struct ata_identify
{
    uint16_t raw[256];
};

struct ata_device
{
    enum ata_bus       bus;
    int                drive;
    enum ata_drive_type type;
    int                present;
    int                lba48;
    uint64_t           sectors;
    struct ata_identify ident;
    char               model[41];
    int                bmdma_avail;
    uint16_t           bmdma_base;
    void              *prdt_phys;
    int                prdt_entries;
};

int  ata_init(void);
int  ata_device_count(void);
struct ata_device *ata_get_device(int index);
int  ata_identify(struct ata_device *dev);
int  ata_read_sectors(struct ata_device *dev, uint64_t lba, uint16_t count, void *buf);
int  ata_write_sectors(struct ata_device *dev, uint64_t lba, uint16_t count, const void *buf);
int  ata_flush(struct ata_device *dev);
int  ata_soft_reset(struct ata_device *dev);
int  ata_read_dma(struct ata_device *dev, uint64_t lba, uint16_t count, void *buf);
int  ata_write_dma(struct ata_device *dev, uint64_t lba, uint16_t count, const void *buf);

#endif
