#include "pci.h"
#include "ports.h"
#include "drivers/serial/serial.h"

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = 0x80000000U
                     | ((uint32_t)bus   << 16)
                     | ((uint32_t)slot  << 11)
                     | ((uint32_t)func  << 8)
                     | (offset & 0xFC);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read_vendor(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint16_t)(pci_read32(bus, slot, func, 0) & 0xFFFF);
}

uint16_t pci_read_device(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint16_t)((pci_read32(bus, slot, func, 0) >> 16) & 0xFFFF);
}

static uint8_t pci_read_header_type(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint8_t)((pci_read32(bus, slot, func, 0x0C) >> 16) & 0xFF);
}

static uint8_t pci_read_class(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint8_t)((pci_read32(bus, slot, func, 0x08) >> 24) & 0xFF);
}

static uint8_t pci_read_subclass(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint8_t)((pci_read32(bus, slot, func, 0x08) >> 16) & 0xFF);
}

static uint8_t pci_read_secondary_bus(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint8_t)((pci_read32(bus, slot, func, 0x18) >> 8) & 0xFF);
}

static void pci_scan_bus(uint8_t bus)
{
    for (int slot = 0; slot < 32; slot++)
    {
        uint16_t vendor = pci_read_vendor(bus, slot, 0);
        if (vendor == 0xFFFF)
            continue;

        int max_func = 1;
        uint8_t header_type = pci_read_header_type(bus, slot, 0);
        if (header_type & 0x80)
            max_func = 8;

        for (int func = 0; func < max_func; func++)
        {
            if (func > 0)
            {
                vendor = pci_read_vendor(bus, slot, func);
                if (vendor == 0xFFFF)
                    continue;
            }

            uint16_t device = pci_read_device(bus, slot, func);
            uint8_t class_code = pci_read_class(bus, slot, func);
            uint8_t subclass = pci_read_subclass(bus, slot, func);

            serial_printf("PCI: %02x:%02x.%d vendor=%04x device=%04x class=%02x subclass=%02x\n",
                          bus, slot, func, vendor, device, class_code, subclass);

            if (class_code == 0x06 && subclass == 0x04)
            {
                uint8_t secondary_bus = pci_read_secondary_bus(bus, slot, func);
                serial_printf("PCI: bridge at %02x:%02x.%d -> bus %02x\n",
                              bus, slot, func, secondary_bus);
                pci_scan_bus(secondary_bus);
            }
        }
    }
}

void pci_scan(void)
{
    serial_write("PCI: scanning buses...\n");

    uint16_t vendor = pci_read_vendor(0, 0, 0);
    if (vendor == 0xFFFF)
    {
        serial_write("PCI: no host bridge at 00:00.0\n");
        return;
    }

    uint8_t header_type = pci_read_header_type(0, 0, 0);
    int max_bus = 1;
    if (header_type & 0x80)
    {
        max_bus = 256;
        serial_write("PCI: multi-bus host controller detected\n");
    }

    for (int bus = 0; bus < max_bus; bus++)
        pci_scan_bus(bus);

    serial_write("PCI: scan complete\n");
}