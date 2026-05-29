#include "rtc.h"
#include "ports.h"
#include "fs/vfs.h"
#include "include/initcall.h"

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

static int rtc_chr_read(struct vfs_inode *inode, uint64_t offset, uint64_t count, void *buf)
{
    (void)inode; (void)offset;
    struct rtc_time tm;
    if (rtc_read(&tm) != 0)
        return -1;
    uint64_t sz = sizeof(tm);
    if (count < sz) sz = count;
    for (uint64_t i = 0; i < sz; i++)
        ((unsigned char *)buf)[i] = ((const unsigned char *)&tm)[i];
    return (int)sz;
}

static struct vfs_inode_ops rtc_chr_ops = {
    .read  = rtc_chr_read,
    .write = NULL,
};

static int rtc_chr_init(void)
{
    devfs_register_chrdev("rtc", &rtc_chr_ops, NULL);
    return 0;
}
pure_initcall(rtc_chr_init);

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