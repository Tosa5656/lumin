#ifndef PCI_H
#define PCI_H

#include <stdint.h>

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read_vendor(uint8_t bus, uint8_t slot, uint8_t func);
uint16_t pci_read_device(uint8_t bus, uint8_t slot, uint8_t func);
void pci_scan(void);

#endif