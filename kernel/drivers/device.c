#include "include/device.h"
#include "../drivers/serial/serial.h"
#include <stddef.h>

static struct bus_type *busses[16];
static int num_busses;

static struct device_driver *drivers[32];
static int num_drivers;

int bus_register(struct bus_type *bus)
{
    if (num_busses >= 16) return -1;
    busses[num_busses++] = bus;
    serial_printf("device: registered bus '%s'\n", bus->name);
    return 0;
}

int driver_register(struct device_driver *drv)
{
    if (num_drivers >= 32) return -1;
    drivers[num_drivers++] = drv;
    serial_printf("device: registered driver '%s' on bus '%s'\n",
                  drv->name, drv->bus ? drv->bus->name : "none");
    return 0;
}

int device_register(struct device *dev)
{
    if (!dev || !dev->bus) return -1;

    if (dev->bus->match)
    {
        for (int i = 0; i < num_drivers; i++)
        {
            if (drivers[i]->bus != dev->bus) continue;
            if (dev->bus->match(dev, drivers[i]))
            {
                dev->driver = drivers[i];
                if (dev->bus->probe)
                    return dev->bus->probe(dev);
                if (drivers[i]->probe)
                    return drivers[i]->probe(dev);
                return 0;
            }
        }
    }

    serial_printf("device: no driver for '%s'\n", dev->name);
    return -1;
}

struct device_driver *driver_find(struct device *dev)
{
    if (!dev || !dev->bus) return NULL;
    for (int i = 0; i < num_drivers; i++)
    {
        if (drivers[i]->bus == dev->bus && dev->bus->match &&
            dev->bus->match(dev, drivers[i]))
            return drivers[i];
    }
    return NULL;
}