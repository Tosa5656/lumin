#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include "../../include/device.h"

#define PCI_MAX_DEVICES 64

struct pci_device {
    struct device dev;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint32_t bar[6];
    uint8_t irq;
};

struct pci_device_id {
    uint16_t vendor;
    uint16_t device;
    uint16_t subvendor;
    uint16_t subdevice;
    uint32_t class;
    uint32_t class_mask;
};

struct pci_driver {
    struct device_driver drv;
    const struct pci_device_id *id_table;
    int (*probe)(struct pci_device *pdev);
    void (*remove)(struct pci_device *pdev);
};

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read_vendor(uint8_t bus, uint8_t slot, uint8_t func);
uint16_t pci_read_device(uint8_t bus, uint8_t slot, uint8_t func);
void pci_scan(void);
int pci_register_driver(struct pci_driver *drv);
struct pci_device *pci_find_device(uint16_t vendor, uint16_t device);
struct pci_device *pci_find_device_by_class(uint8_t class_code, uint8_t subclass);
int pci_device_count(void);
struct pci_device *pci_get_device(int index);

#endif
