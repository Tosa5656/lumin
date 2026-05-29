#ifndef _TIME_H
#define _TIME_H

struct timespec {
    unsigned long tv_sec;
    unsigned long tv_nsec;
};

int clock_gettime(int clk_id, struct timespec *tp);

#endif
