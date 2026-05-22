#ifndef HPET_H
#define HPET_H

#include <stdint.h>
#include <stddef.h>

int      hpet_init(uint32_t hz);
void     hpet_tick(void);
uint64_t hpet_get_ticks(void);

#endif