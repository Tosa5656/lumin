#include "fb.h"
#include "fb_font.h"
#include "../../mm/vmm.h"
#include "../../mm/pmm.h"
#include "../../kprintf.h"

#define FB_VADDR      0xFFFF8000FC000000ULL
#define FB_CHAR_W     8
#define FB_CHAR_H     16
#define FB_COLS       (fb_width / FB_CHAR_W)
#define FB_ROWS       (fb_height / FB_CHAR_H)
#define FB_DEFAULT_FG 0x00C0C0C0
#define FB_DEFAULT_BG 0x00000000

int      fb_available = 0;
int      fb_width = 0;
int      fb_height = 0;

static uint64_t fb_vaddr = 0;
static int      fb_pitch = 0;
static int      fb_row = 0;
static int      fb_col = 0;
static uint32_t fb_fg = FB_DEFAULT_FG;
static uint32_t fb_bg = FB_DEFAULT_BG;

static uint32_t fb_cursor_save[FB_CHAR_H][FB_CHAR_W];
static int      fb_cursor_row = 0;
static int      fb_cursor_col = 0;
static int      fb_cursor_on = 0;

struct fb_info {
    uint64_t phys_addr;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
};

static void fb_scroll(void);

static void fb_cursor_erase(void)
{
    if (!fb_cursor_on) return;
    uint32_t cx = fb_cursor_col * FB_CHAR_W;
    uint32_t cy = fb_cursor_row * FB_CHAR_H;
    for (int r = 0; r < FB_CHAR_H; r++)
    {
        volatile uint32_t *line = (volatile uint32_t *)(fb_vaddr + (cy + r) * fb_pitch);
        for (int col = 0; col < FB_CHAR_W; col++)
            line[cx + col] = fb_cursor_save[r][col];
    }
    fb_cursor_on = 0;
}

static void fb_cursor_draw(void)
{
    fb_cursor_erase();

    fb_cursor_row = fb_row;
    fb_cursor_col = fb_col;
    uint32_t cx = fb_col * FB_CHAR_W;
    uint32_t cy = fb_row * FB_CHAR_H;
    for (int r = 0; r < FB_CHAR_H; r++)
    {
        volatile uint32_t *line = (volatile uint32_t *)(fb_vaddr + (cy + r) * fb_pitch);
        for (int col = 0; col < FB_CHAR_W; col++)
            fb_cursor_save[r][col] = line[cx + col];
    }

    uint32_t cursor_color = 0x00C0C0C0;
    for (int r = FB_CHAR_H - 2; r < FB_CHAR_H; r++)
    {
        volatile uint32_t *line = (volatile uint32_t *)(fb_vaddr + (cy + r) * fb_pitch);
        for (int col = 0; col < FB_CHAR_W; col++)
            line[cx + col] = cursor_color;
    }

    fb_cursor_on = 1;
}

void fb_init(void)
{
    volatile struct fb_info *info = (volatile struct fb_info *)0x7E00;

    if (info->phys_addr == 0 || info->width == 0)
    {
        fb_available = 0;
        return;
    }

    fb_width  = info->width;
    fb_height = info->height;
    fb_pitch  = info->pitch;

    uint32_t size = fb_pitch * fb_height;
    uint32_t pages = (size + 0xFFF) / 0x1000;

    uint64_t cr3 = vmm_read_cr3();
    vmm_map_range((uint64_t *)(uintptr_t)cr3, FB_VADDR, info->phys_addr, pages,
                  PAGE_PRESENT | PAGE_WRITE);

    fb_vaddr = FB_VADDR;
    fb_available = 1;

    fb_clear(fb_bg);
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= (uint32_t)fb_width || y >= (uint32_t)fb_height || !fb_vaddr)
        return;
    volatile uint32_t *p = (volatile uint32_t *)(fb_vaddr + y * fb_pitch + x * 4);
    *p = color;
}

void fb_fillrect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (!fb_vaddr) return;
    for (uint32_t row = 0; row < h; row++)
    {
        volatile uint32_t *line = (volatile uint32_t *)(fb_vaddr + (y + row) * fb_pitch);
        for (uint32_t col = 0; col < w; col++)
            line[x + col] = color;
    }
}

void fb_putchar(char c)
{
    if (!fb_available) return;

    if (c == '\n')
    {
        fb_col = 0;
        if (++fb_row >= FB_ROWS) fb_scroll();
        fb_cursor_draw();
        return;
    }
    if (c == '\r')
    {
        fb_col = 0;
        fb_cursor_draw();
        return;
    }
    if (c == '\t')
    {
        int stop = (fb_col + 4) & ~3;
        while (fb_col < stop && fb_col < FB_COLS)
            fb_putchar(' ');
        return;
    }
    if (c == '\b')
    {
        if (fb_col > 0) fb_col--;
        fb_cursor_draw();
        return;
    }

    if ((unsigned char)c < 32) return;

    fb_cursor_erase();

    uint32_t cx = fb_col * FB_CHAR_W;
    uint32_t cy = fb_row * FB_CHAR_H;

    for (int r = 0; r < FB_CHAR_H; r++)
    {
        uint8_t glyph = fb_font[(unsigned char)c][r];
        volatile uint32_t *line = (volatile uint32_t *)(fb_vaddr + (cy + r) * fb_pitch);
        for (int col = 0; col < FB_CHAR_W; col++)
            line[cx + col] = (glyph & (0x80 >> col)) ? fb_fg : fb_bg;
    }

    if (++fb_col >= FB_COLS)
    {
        fb_col = 0;
        if (++fb_row >= FB_ROWS) fb_scroll();
    }
    fb_cursor_draw();
}

static void fb_scroll(void)
{
    fb_cursor_erase();
    uint32_t line_bytes = fb_pitch * FB_CHAR_H;
    int rows = fb_height / FB_CHAR_H;

    for (int r = 1; r < rows; r++)
    {
        uint64_t src = fb_vaddr + r * line_bytes;
        uint64_t dst = fb_vaddr + (r - 1) * line_bytes;
        for (uint32_t i = 0; i < line_bytes; i += 8)
            *(volatile uint64_t *)(dst + i) = *(volatile uint64_t *)(src + i);
    }

    uint32_t last_y = (rows - 1) * FB_CHAR_H;
    fb_fillrect(0, last_y, fb_width, FB_CHAR_H, fb_bg);
    fb_row = rows - 1;
}

void fb_puts(const char *str)
{
    while (*str) fb_putchar(*str++);
}

static void fb_putch(char c, void *ctx)
{
    (void)ctx;
    fb_putchar(c);
}

void fb_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvformat(fb_putch, NULL, fmt, ap);
    va_end(ap);
}

void fb_clear(uint32_t color)
{
    fb_cursor_on = 0;
    fb_fillrect(0, 0, fb_width, fb_height, color);
    fb_row = 0;
    fb_col = 0;
}

uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}