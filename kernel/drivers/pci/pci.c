#include "pci.h"
#include "../../ports.h"
#include "../serial/serial.h"

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = 0x80000000U
                     | ((uint32_t)bus   << 16)
                     | ((uint32_t)slot  << 11)
                     | ((uint32_t)func  << 8)
                     | (offset & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}

uint16_t pci_read_vendor(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint16_t)(pci_read32(bus, slot, func, 0) & 0xFFFF);
}

uint16_t pci_read_device(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint16_t)((pci_read32(bus, slot, func, 0) >> 16) & 0xFFFF);
}

void pci_scan(void)
{
    serial_write("PCI: scanning bus 0...\n");
    int found = 0;
    for (int slot = 0; slot < 32; slot++) {
        uint16_t vendor = pci_read_vendor(0, slot, 0);
        if (vendor == 0xFFFF) continue;
        uint16_t device = pci_read_device(0, slot, 0);
        uint32_t class_reg = pci_read32(0, slot, 0, 8);
        uint8_t class_code = (class_reg >> 24) & 0xFF;
        uint8_t subclass  = (class_reg >> 16) & 0xFF;
        serial_printf("PCI: %02x:%02x.%d vendor=%04x device=%04x class=%02x subclass=%02x\n",
                     0, slot, 0, vendor, device, class_code, subclass);
        found++;
    }
    serial_printf("PCI: found %d devices\n", found);
}