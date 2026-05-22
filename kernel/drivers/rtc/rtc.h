#ifndef RTC_H
#define RTC_H

#include <stdint.h>

struct rtc_time
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

int rtc_read(struct rtc_time *tm);

#endif