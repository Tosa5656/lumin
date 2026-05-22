#ifndef KMALLOC_H
#define KMALLOC_H

#include <stdint.h>
#include <stddef.h>

void kmalloc_init(void);
void *kmalloc(uint64_t size);
void kfree(void *ptr);

#endif