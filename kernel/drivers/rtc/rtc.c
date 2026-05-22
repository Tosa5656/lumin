#include "rtc.h"
#include "../../ports.h"

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71

static int cmos_read(uint8_t reg)
{
    outb(CMOS_INDEX, reg | 0x80);
    return inb(CMOS_DATA);
}

static int bcd_to_bin(int bcd)
{
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

int rtc_read(struct rtc_time *tm)
{
    uint8_t statusB = cmos_read(0x0B);
    int binary = statusB & 0x04;

    while (cmos_read(0x0A) & 0x80);

    int second = cmos_read(0x00);
    int minute = cmos_read(0x02);
    int hour   = cmos_read(0x04);
    int day    = cmos_read(0x07);
    int month  = cmos_read(0x08);
    int year   = cmos_read(0x09);
    int century= cmos_read(0x32);

    outb(CMOS_INDEX, 0x00);

    if (!binary)
    {
        second = bcd_to_bin(second);
        minute = bcd_to_bin(minute);
        hour   = bcd_to_bin(hour);
        day    = bcd_to_bin(day);
        month  = bcd_to_bin(month);
        year   = bcd_to_bin(year);
        century= bcd_to_bin(century);
    }

    int full_year = century ? century * 100 + year : 2000 + year;
    tm->second = second;
    tm->minute = minute;
    tm->hour   = hour;
    tm->day    = day;
    tm->month  = month;
    tm->year   = full_year;

    return 0;
}