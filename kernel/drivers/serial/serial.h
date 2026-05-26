#ifndef SERIAL_H
#define SERIAL_H

#define COM1 0x3F8

void serial_init(void);
void serial_putchar(char c);
void serial_write(const char *str);
void serial_printf(const char *fmt, ...);
int  serial_readchar(void);

#endif