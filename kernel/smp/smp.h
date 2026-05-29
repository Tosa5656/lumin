#ifndef SMP_H
#define SMP_H

#include <stdint.h>

void smp_init(void);
void smp_bringup_aps(void);
int  smp_cpu_count(void);

#endif