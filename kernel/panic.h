#ifndef PANIC_H
#define PANIC_H

#include "drivers/vga/vga.h"

#define PANIC_FG VGA_WHITE
#define PANIC_BG VGA_LIGHT_RED

void panic(const char *title, const char *msg);

#endif