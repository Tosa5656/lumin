#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>

#define IORESOURCE_IO   1
#define IORESOURCE_MEM  2
#define IORESOURCE_IRQ  4

struct resource {
    const char *name;
    uint64_t start;
    uint64_t end;
    unsigned long flags;
};

struct device_driver;
struct device;

struct bus_type {
    const char *name;
    int (*match)(struct device *dev, struct device_driver *drv);
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
};

struct device_driver {
    const char *name;
    struct bus_type *bus;
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
};

struct device {
    const char *name;
    struct device *parent;
    struct bus_type *bus;
    struct device_driver *driver;
    void *priv;
    struct resource *resources;
    int num_resources;
};

int device_register(struct device *dev);
int driver_register(struct device_driver *drv);
int bus_register(struct bus_type *bus);

struct device_driver *driver_find(struct device *dev);

#endif