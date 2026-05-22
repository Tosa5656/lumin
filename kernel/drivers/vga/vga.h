#ifndef VGA_H
#define VGA_H

#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

enum vga_color
{
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_LIGHT_BROWN   = 14,
    VGA_WHITE         = 15,
};

static inline unsigned char vga_entry_color(enum vga_color fg, enum vga_color bg)
{
    return fg | bg << 4;
}

static inline unsigned short vga_entry(unsigned char c, unsigned char color)
{
    return (unsigned short)c | (unsigned short)color << 8;
}

void vga_init(void);
void vga_clear(unsigned char color);
void vga_putchar(char c, unsigned char color);
void vga_write(const char* str, unsigned char color);
void vga_backspace(void);
void vga_set_cursor(int row, int col);
void vga_printf(const char *fmt, ...);

extern unsigned char vga_default_color;

#endif