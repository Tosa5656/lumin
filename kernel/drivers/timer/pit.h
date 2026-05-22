#ifndef PIT_H
#define PIT_H

#include <stdint.h>

void     pit_init(uint32_t hz);
void     pit_stop(void);
void     pit_tick(void);
uint64_t pit_get_ticks(void);

#endif