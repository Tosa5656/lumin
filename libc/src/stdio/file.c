#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>

FILE __stdin;
FILE __stdout;
FILE __stderr;

static unsigned char stdio_buf[3][BUFSIZ];
static int stdio_inited = 0;

static void stdio_init(void)
{
    if (stdio_inited) return;
    stdio_inited = 1;

    __stdin.fd = 0;
    __stdin.buf = stdio_buf[0];
    __stdin.bufsiz = BUFSIZ;
    __stdin.len = 0;
    __stdin.pos = 0;
    __stdin.flags = _IO_READING;
    __stdin.has_ungetc = 0;

    __stdout.fd = 1;
    __stdout.buf = stdio_buf[1];
    __stdout.bufsiz = BUFSIZ;
    __stdout.pos = 0;
    __stdout.len = 0;
    __stdout.flags = _IO_WRITING;
    __stdout.has_ungetc = 0;

    __stderr.fd = 2;
    __stderr.buf = stdio_buf[2];
    __stderr.bufsiz = BUFSIZ;
    __stderr.pos = 0;
    __stderr.len = 0;
    __stderr.flags = _IO_WRITING | _IO_UNBUFFERED;
    __stderr.has_ungetc = 0;
}

FILE *fopen(const char *path, const char *mode)
{
    int flags = 0;
    int rw = 0;

    while (*mode)
    {
        switch (*mode)
        {
            case 'r': if (!rw) rw = O_RDONLY; break;
            case 'w': if (!rw) rw = O_WRONLY; flags |= O_CREAT | O_TRUNC; break;
            case 'a': if (!rw) rw = O_WRONLY; flags |= O_CREAT | O_APPEND; break;
            case '+': if (rw == O_RDONLY) rw = O_RDWR; else rw = O_RDWR; break;
            case 'b': break;
        }
        mode++;
    }

    if (rw == 0) return NULL;
    flags |= rw;

    int fd = open(path, flags);
    if (fd < 0) return NULL;

    FILE *f = (FILE *)malloc(sizeof(FILE));
    if (!f) { close(fd); return NULL; }

    f->buf = (unsigned char *)malloc(BUFSIZ);
    if (!f->buf) { free(f); close(fd); return NULL; }

    f->fd = fd;
    f->bufsiz = BUFSIZ;
    f->pos = 0;
    f->len = 0;
    f->flags = (rw == O_RDONLY) ? _IO_READING : _IO_WRITING;
    f->has_ungetc = 0;

    return f;
}

int fclose(FILE *f)
{
    if (!f) return EOF;
    if (f == stdin || f == stdout || f == stderr)
        return fflush(f);

    int ret = fflush(f);
    close(f->fd);
    free(f->buf);
    free(f);
    return ret;
}

int fflush(FILE *f)
{
    if (!f || !(f->flags & _IO_WRITING))
        return 0;
    if (f->pos == 0)
        return 0;

    unsigned long total = f->pos;
    unsigned char *p = f->buf;
    while (total > 0)
    {
        ssize_t n = write(f->fd, p, total);
        if (n <= 0)
        {
            f->flags |= _IO_ERROR;
            return EOF;
        }
        p += n;
        total -= n;
    }
    f->pos = 0;
    return 0;
}

int setvbuf(FILE *f, char *buf, int mode, unsigned long size)
{
    if (mode == _IONBF)
    {
        f->flags |= _IO_UNBUFFERED;
        return 0;
    }
    if (mode == _IOLBF)
        f->flags |= _IO_LINEBUF;
    if (buf && size > 0)
    {
        f->buf = (unsigned char *)buf;
        f->bufsiz = size;
    }
    return 0;
}

unsigned long fread(void *ptr, unsigned long size, unsigned long nmemb, FILE *f)
{
    if (!(f->flags & _IO_READING))
        return 0;

    unsigned long total = size * nmemb;
    unsigned long done = 0;
    unsigned char *p = (unsigned char *)ptr;

    while (done < total)
    {
        if (f->pos >= f->len)
        {
            if (f->flags & _IO_EOF)
                break;
            f->pos = 0;
            ssize_t n = read(f->fd, f->buf, f->bufsiz);
            if (n <= 0)
            {
                if (n == 0)
                    f->flags |= _IO_EOF;
                else
                    f->flags |= _IO_ERROR;
                break;
            }
            f->len = (unsigned long)n;
        }

        unsigned long avail = f->len - f->pos;
        unsigned long chunk = total - done;
        if (chunk > avail) chunk = avail;

        for (unsigned long i = 0; i < chunk; i++)
            p[done + i] = f->buf[f->pos + i];
        f->pos += chunk;
        done += chunk;
    }

    return size > 0 ? done / size : 0;
}

unsigned long fwrite(const void *ptr, unsigned long size, unsigned long nmemb, FILE *f)
{
    if (!(f->flags & _IO_WRITING))
        return 0;

    unsigned long total = size * nmemb;
    const unsigned char *p = (const unsigned char *)ptr;
    unsigned long written = 0;

    while (written < total)
    {
        unsigned long remaining = total - written;
        unsigned long space = f->bufsiz - f->pos;
        unsigned long chunk = remaining < space ? remaining : space;

        for (unsigned long i = 0; i < chunk; i++)
            f->buf[f->pos + i] = p[written + i];
        f->pos += chunk;
        written += chunk;

        if (f->pos >= f->bufsiz)
        {
            if (fflush(f) != 0)
                return size > 0 ? written / size : 0;
        }
    }

    return nmemb;
}
