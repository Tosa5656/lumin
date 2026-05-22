#ifndef E820_H
#define E820_H

#include <stdint.h>

#define E820_AVAILABLE   1
#define E820_RESERVED    2
#define E820_ACPI_RECLAIM 3
#define E820_ACPI_NVS    4
#define E820_BAD         5

struct e820_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
} __attribute__((packed));

#define E820_COUNT_ADDR ((uint16_t *)0x5FFC)
#define E820_ENTRIES    ((struct e820_entry *)0x6000)

#endif
