#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define BUFSIZ 1024
#define EOF (-1)

#define _IO_UNUSED      0
#define _IO_READING     1
#define _IO_WRITING     2
#define _IO_EOF         4
#define _IO_ERROR       8
#define _IO_UNBUFFERED  16
#define _IO_LINEBUF     32

#define _IONBF 0
#define _IOLBF 1
#define _IOFBF 2

typedef struct _FILE {
    int fd;
    unsigned char *buf;
    unsigned long bufsiz;
    unsigned long pos;
    unsigned long len;
    int flags;
    int last_ungetc;
    int has_ungetc;
} FILE;

extern FILE __stdin;
extern FILE __stdout;
extern FILE __stderr;

#define stdin  (&__stdin)
#define stdout (&__stdout)
#define stderr (&__stderr)

FILE *fopen(const char *path, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *f);
int fclose(FILE *f);
int fflush(FILE *f);
int setvbuf(FILE *f, char *buf, int mode, unsigned long size);

unsigned long fread(void *ptr, unsigned long size, unsigned long nmemb, FILE *f);
unsigned long fwrite(const void *ptr, unsigned long size, unsigned long nmemb, FILE *f);

int fgetc(FILE *f);
char *fgets(char *s, int n, FILE *f);
int ungetc(int c, FILE *f);

int fputc(int c, FILE *f);
int fputs(const char *s, FILE *f);

int feof(FILE *f);
int ferror(FILE *f);
void clearerr(FILE *f);

int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, unsigned long n, const char *fmt, ...);
int vfprintf(FILE *f, const char *fmt, va_list ap);
int vsprintf(char *buf, const char *fmt, va_list ap);
int vsnprintf(char *buf, unsigned long n, const char *fmt, va_list ap);

int scanf(const char *fmt, ...);
int fscanf(FILE *f, const char *fmt, ...);
int sscanf(const char *buf, const char *fmt, ...);

void vcb_printf(const char *fmt, va_list ap, void (*out)(char, void *), void *ctx);

int putchar(int c);
int puts(const char *s);
int remove(const char *path);
int rename(const char *old, const char *new_);

#endif
