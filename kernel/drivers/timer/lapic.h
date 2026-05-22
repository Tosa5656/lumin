#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>

int      lapic_init(uint32_t hz);
void     lapic_tick(void);
uint64_t lapic_get_ticks(void);
void     lapic_eoi(void);
uint64_t lapic_get_base(void);

#endif
