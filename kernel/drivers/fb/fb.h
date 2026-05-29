#ifndef FB_H
#define FB_H

#include <stdint.h>

void     fb_init(void);
void     fb_putpixel(uint32_t x, uint32_t y, uint32_t color);
void     fb_fillrect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void     fb_putchar(char c);
void     fb_puts(const char *str);
void     fb_printf(const char *fmt, ...);
void     fb_clear(uint32_t color);
uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b);

extern int  fb_available;
extern int  fb_width;
extern int  fb_height;

#endif