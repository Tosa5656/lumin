#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int fputc(int c, FILE *f)
{
    if (!(f->flags & _IO_WRITING))
        return EOF;

    if (f->flags & _IO_UNBUFFERED)
    {
        unsigned char ch = (unsigned char)c;
        if (write(f->fd, &ch, 1) != 1)
        {
            f->flags |= _IO_ERROR;
            return EOF;
        }
        return (unsigned char)c;
    }

    f->buf[f->pos++] = (unsigned char)c;

    if (f->pos >= f->bufsiz ||
        ((f->flags & _IO_LINEBUF) && c == '\n'))
    {
        if (fflush(f) != 0)
            return EOF;
    }

    return (unsigned char)c;
}

int fputs(const char *s, FILE *f)
{
    while (*s)
    {
        if (fputc((unsigned char)*s, f) == EOF)
            return EOF;
        s++;
    }
    return 1;
}

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    vcb_printf(fmt, ap, (void (*)(char, void *))fputc, f);
    return 0;
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    return 0;
}
