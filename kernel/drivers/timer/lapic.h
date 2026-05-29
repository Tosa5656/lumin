#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>

#define LAPIC_ID         0x20
#define LAPIC_ICR_LOW    0x300
#define LAPIC_ICR_HIGH   0x310

#define IPI_INIT         0x500
#define IPI_STARTUP      0x600
#define IPI_FIXED        0x400

int      lapic_init(uint32_t hz);
void     lapic_tick(void);
uint64_t lapic_get_ticks(void);
void     lapic_eoi(void);
uint64_t lapic_get_base(void);
uint8_t  lapic_read_id(void);
void     lapic_send_ipi(uint8_t apic_id, uint32_t type, uint8_t vector);
void     lapic_enable(void);

#endif
