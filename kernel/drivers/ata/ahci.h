#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>

/* HBA generic host control registers (offset from ABAR) */
#define HBA_CAP       0x00
#define HBA_GHC       0x04
#define HBA_IS        0x08
#define HBA_PI        0x0C
#define HBA_VS        0x10
#define HBA_CAP2      0x24
#define HBA_BOHC      0x28

/* HBA_GHC bits */
#define GHC_HR        (1U << 0)
#define GHC_IE        (1U << 1)
#define GHC_AE        (1U << 31)

/* CAP bits */
#define CAP_NP_SHIFT  0
#define CAP_NP_MASK   0x1F
#define CAP_NCS_SHIFT 8
#define CAP_NCS_MASK  (0x1F << 8)
#define CAP_SSS       (1U << 27)
#define CAP_S64A      (1U << 31)

/* CAP2 bits */
#define CAP2_BOH      (1U << 0)
#define CAP2_NVMP     (1U << 1)

/* Port register offsets (relative to PxBase = 0x100 + port * 0x80) */
#define PORT_CLB      0x00
#define PORT_CLBU     0x04
#define PORT_FB       0x08
#define PORT_FBU      0x0C
#define PORT_IS       0x10
#define PORT_IE       0x14
#define PORT_CMD      0x18
#define PORT_RESERVED 0x1C
#define PORT_TFD      0x20
#define PORT_SIG      0x24
#define PORT_SSTS     0x28
#define PORT_SCTL     0x2C
#define PORT_SERR     0x30
#define PORT_SACT     0x34
#define PORT_CI       0x38
#define PORT_SNTF     0x3C
#define PORT_FBS      0x40

/* PORT_CMD bits */
#define PORT_CMD_ST       (1U << 0)
#define PORT_CMD_SUD      (1U << 1)
#define PORT_CMD_POD      (1U << 2)
#define PORT_CMD_CLO      (1U << 3)
#define PORT_CMD_FRE      (1U << 4)
#define PORT_CMD_CR       (1U << 15)
#define PORT_CMD_FR       (1U << 14)
#define PORT_CMD_ATAPI    (1U << 24)
#define PORT_CMD_ESP      (1U << 16)
#define PORT_CMD_CPD      (1U << 20)
#define PORT_CMD_MPSP     (1U << 13)
#define PORT_CMD_HPCP     (1U << 18)
#define PORT_CMD_PMA      (1U << 17)
#define PORT_CMD_ICC_MASK (0xF << 28)
#define PORT_CMD_ICC_ACTIVE (0x1 << 28)

/* PORT_TFD bits */
#define TFD_BSY   (1U << 7)
#define TFD_DRQ   (1U << 3)
#define TFD_ERR   (1U << 0)

/* PORT_SSTS bits */
#define SSTS_DET_MASK  0x0F
#define SSTS_DET_NODEV 0x00
#define SSTS_DET_PHY   0x01
#define SSTS_DET_PME   0x02
#define SSTS_DET_PRES  0x03
#define SSTS_SPD_MASK  0xF0
#define SSTS_IPM_MASK  0x0F00
#define SSTS_IPM_ACTIVE 0x0100

/* PORT_SERR bits to clear */
#define SERR_CLEAR    0xFFFFFFFF

/* FIS types */
#define FIS_TYPE_H2D       0x27
#define FIS_TYPE_D2H       0x34
#define FIS_TYPE_SDB       0xA1
#define FIS_TYPE_DMA_ACT   0x39
#define FIS_TYPE_DMA_SETUP 0x41
#define FIS_TYPE_PIO_SETUP 0x5F

/* ATA commands */
#define ATA_CMD_READ_DMA_EXT    0x25
#define ATA_CMD_WRITE_DMA_EXT   0x35
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_FLUSH_CACHE     0xE7
#define ATA_CMD_FLUSH_CACHE_EXT 0xEA

/* SATA signatures (read from PxSIG after reset) */
#define SATA_SIG_ATA    0x00000101
#define SATA_SIG_ATAPI  0xEB140101
#define SATA_SIG_PM     0x96690104
#define SATA_SIG_NONE   0xFFFFFFFF

/* Command header DW0 layout */
#define CMDH_CFIS_LEN(n)  ((n) & 0x1F)
#define CMDH_ATAPI         (1U << 5)
#define CMDH_WRITE         (1U << 6)
#define CMDH_PREFETCH      (1U << 7)
#define CMDH_RESET         (1U << 8)
#define CMDH_BIST          (1U << 9)
#define CMDH_CLR_BUSY      (1U << 10)
#define CMDH_PRDTL(n)      (((n) & 0xFFFF) << 16)

/* PRDT entry (16 bytes) */
struct ahci_prdt {
    uint32_t dba;       /* data base address, low */
    uint32_t dbau;      /* data base address, high */
    uint32_t reserved;
    uint32_t dbc;       /* byte count - 1, bit 31 = IOC */
};

/* Command header (32 bytes) */
struct ahci_cmd_header {
    uint32_t opts;      /* DW0: PRDTL | flags | CFIS length */
    uint32_t opts2;     /* DW1: DBC | PM | flags */
    uint32_t ctba;      /* DW2: command table base address, low */
    uint32_t ctbau;     /* DW3: command table base address, high */
    uint32_t reserved[4];
};

/* Command table (variable length, 128-byte aligned) */
struct ahci_cmd_table {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  reserved[48];
    struct ahci_prdt prdt[];
};

/* Register H2D FIS (20 bytes) */
struct __attribute__((packed)) fis_reg_h2d {
    uint8_t  fis_type;      /* 0x27 */
    uint8_t  flags;         /* bit 7 = C, bit 0 = W */
    uint8_t  command;
    uint8_t  features_low;
    uint8_t  lba0;
    uint8_t  lba1;
    uint8_t  lba2;
    uint8_t  device;
    uint8_t  lba3;
    uint8_t  lba4;
    uint8_t  lba5;
    uint8_t  features_high;
    uint8_t  count_low;
    uint8_t  count_high;
    uint8_t  icc;
    uint8_t  control;
    uint8_t  reserved[4];
};

/* Check that FIS is 20 bytes */
_Static_assert(sizeof(struct fis_reg_h2d) == 20, "fis_reg_h2d size wrong");

/* Port private data */
struct ahci_device {
    int              present;
    int              port;
    uint64_t         sectors;
    int              lba48;
    char             model[41];
    uint32_t         sig;

    /* Allocated memory (identity-mapped: virt == phys) */
    struct ahci_cmd_header *clb;
    void                   *fb;
    struct ahci_cmd_table  *ct;
};

/* AHCI HBA state */
struct ahci_hba {
    volatile void    *abar;
    uint32_t          cap;
    uint32_t          cap2;
    uint32_t          pi;
    int               n_ports;
    int               n_slots;
    struct ahci_device devices[32];
};

/* Driver entry point */
int ahci_init(void);

#endif
