#include "rtc.h"
#include "ports.h"

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

    int second, minute, hour, day, month, year, century;

    for (int retry = 0; retry < 3; retry++)
    {
        while (cmos_read(0x0A) & 0x80);

        second = cmos_read(0x00);
        minute = cmos_read(0x02);
        hour   = cmos_read(0x04);
        day    = cmos_read(0x07);
        month  = cmos_read(0x08);
        year   = cmos_read(0x09);
        century= cmos_read(0x32);

        if (!(cmos_read(0x0A) & 0x80))
        {
            int s2 = cmos_read(0x00);
            int m2 = cmos_read(0x02);
            if (s2 == second && m2 == minute)
                break;
        }
    }

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