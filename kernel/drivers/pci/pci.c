#include "pci.h"
#include "ports.h"
#include "drivers/serial/serial.h"
#include <stddef.h>

#define PCI_CONFIG_ADDR  0xCF8
#define PCI_CONFIG_DATA  0xCFC

static struct pci_device pci_devices[PCI_MAX_DEVICES];
static int pci_num_devices;
static struct pci_driver *pci_drivers[16];
static int pci_driver_count;
static struct bus_type pci_bus_type;

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

static uint8_t pci_read_prog_if(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint8_t)((pci_read32(bus, slot, func, 0x08) >> 8) & 0xFF);
}

static uint8_t pci_read_secondary_bus(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint8_t)((pci_read32(bus, slot, func, 0x18) >> 8) & 0xFF);
}

static uint32_t pci_read_bar(uint8_t bus, uint8_t slot, uint8_t func, int bar)
{
    return pci_read32(bus, slot, func, 0x10 + bar * 4);
}

static uint8_t pci_read_irq(uint8_t bus, uint8_t slot, uint8_t func)
{
    return (uint8_t)((pci_read32(bus, slot, func, 0x3C) >> 8) & 0xFF);
}

static int pci_match(struct device *dev, struct device_driver *drv)
{
    struct pci_device *pdev = (struct pci_device *)dev;
    struct pci_driver *pdrv = (struct pci_driver *)drv;
    const struct pci_device_id *id = pdrv->id_table;

    if (!id) return 0;

    while (id->vendor != 0 || id->device != 0)
    {
        if ((id->vendor == pdev->vendor || id->vendor == 0xFFFF) &&
            (id->device == pdev->device_id || id->device == 0xFFFF) &&
            (id->class == (pdev->class_code << 16 | pdev->subclass << 8 | pdev->prog_if) || id->class == 0))
            return 1;
        id++;
    }
    return 0;
}

static int pci_bus_probe(struct device *dev)
{
    struct pci_device *pdev = (struct pci_device *)dev;
    struct pci_driver *pdrv = (struct pci_driver *)dev->driver;

    if (pdrv && pdrv->probe)
        return pdrv->probe(pdev);
    return -1;
}

static int pci_bus_remove(struct device *dev)
{
    struct pci_device *pdev = (struct pci_device *)dev;
    struct pci_driver *pdrv = (struct pci_driver *)dev->driver;

    if (pdrv && pdrv->remove)
        pdrv->remove(pdev);
    return 0;
}

int pci_register_driver(struct pci_driver *drv)
{
    if (pci_driver_count >= 16) return -1;

    drv->drv.bus = &pci_bus_type;
    drv->drv.name = drv->drv.name ? drv->drv.name : "pci_drv";
    drv->drv.probe = pci_bus_probe;
    drv->drv.remove = pci_bus_remove;
    pci_drivers[pci_driver_count++] = drv;
    driver_register(&drv->drv);

    for (int i = 0; i < pci_num_devices; i++)
    {
        if (!pci_devices[i].dev.driver && pci_match(&pci_devices[i].dev, &drv->drv))
        {
            pci_devices[i].dev.driver = &drv->drv;
            serial_printf("pci: driver '%s' matched %02x:%02x.%d\n",
                          drv->drv.name, pci_devices[i].bus,
                          pci_devices[i].slot, pci_devices[i].func);
            if (drv->probe)
                drv->probe(&pci_devices[i]);
        }
    }
    return 0;
}

struct pci_device *pci_find_device(uint16_t vendor, uint16_t device)
{
    for (int i = 0; i < pci_num_devices; i++)
    {
        if (pci_devices[i].vendor == vendor && pci_devices[i].device_id == device)
            return &pci_devices[i];
    }
    return NULL;
}

struct pci_device *pci_find_device_by_class(uint8_t class_code, uint8_t subclass)
{
    for (int i = 0; i < pci_num_devices; i++)
    {
        if (pci_devices[i].class_code == class_code && pci_devices[i].subclass == subclass)
            return &pci_devices[i];
    }
    return NULL;
}

int pci_device_count(void)
{
    return pci_num_devices;
}

struct pci_device *pci_get_device(int index)
{
    if (index < 0 || index >= pci_num_devices)
        return NULL;
    return &pci_devices[index];
}

static void pci_create_device(uint8_t bus, uint8_t slot, uint8_t func,
                               uint16_t vendor, uint16_t device,
                               uint8_t class_code, uint8_t subclass, uint8_t prog_if)
{
    if (pci_num_devices >= PCI_MAX_DEVICES) return;

    struct pci_device *pdev = &pci_devices[pci_num_devices++];
    pdev->bus        = bus;
    pdev->slot       = slot;
    pdev->func       = func;
    pdev->vendor     = vendor;
    pdev->device_id  = device;
    pdev->class_code = class_code;
    pdev->subclass   = subclass;
    pdev->prog_if    = prog_if;

    for (int b = 0; b < 6; b++)
        pdev->bar[b] = pci_read_bar(bus, slot, func, b);

    pdev->irq = pci_read_irq(bus, slot, func);
    pdev->dev.name = "pci_device";
    pdev->dev.bus  = &pci_bus_type;
    pdev->dev.driver = NULL;
    pdev->dev.priv   = NULL;
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
            uint8_t prog_if = pci_read_prog_if(bus, slot, func);

            serial_printf("PCI: %02x:%02x.%d vendor=%04x device=%04x class=%02x subclass=%02x\n",
                          bus, slot, func, vendor, device, class_code, subclass);

            pci_create_device(bus, slot, func, vendor, device, class_code, subclass, prog_if);

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

    pci_bus_type.name  = "pci";
    pci_bus_type.match = pci_match;
    pci_bus_type.probe = pci_bus_probe;
    pci_bus_type.remove = pci_bus_remove;
    bus_register(&pci_bus_type);

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
