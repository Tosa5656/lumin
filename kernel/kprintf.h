#ifndef KPRINTF_H
#define KPRINTF_H

typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)
#define va_arg(v, t)   __builtin_va_arg(v, t)

typedef void (*kputch_t)(char, void *ctx);

void kvformat(kputch_t putch, void *ctx, const char *fmt, va_list ap);
int ksnprintf(char *buf, unsigned long size, const char *fmt, ...);

#endif