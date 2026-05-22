#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

enum timer_type { TIMER_NONE, TIMER_PIT, TIMER_HPET, TIMER_LAPIC };

void            timer_init(uint32_t hz);
uint64_t        timer_get_ticks(void);
void            timer_sleep(uint32_t ms);
void            timer_tick_handler(void);
enum timer_type timer_get_type(void);
void            timer_eoi(void);

#endif