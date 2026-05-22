#ifndef ACPI_COMMON_H
#define ACPI_COMMON_H

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

static inline uint8_t acpi_checksum(const void *data, size_t len)
{
    uint8_t sum = 0;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++)
        sum += p[i];
    return sum;
}

static inline struct rsdp_t *rsdp_find(void)
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

#endif
