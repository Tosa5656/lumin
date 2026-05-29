#include <stdio.h>
#include <unistd.h>

int fgetc(FILE *f)
{
    if (!(f->flags & _IO_READING))
        return EOF;

    if (f->has_ungetc)
    {
        f->has_ungetc = 0;
        return f->last_ungetc;
    }

    if (f->pos >= f->len)
    {
        if (f->flags & _IO_EOF)
            return EOF;
        f->pos = 0;
        ssize_t n = read(f->fd, f->buf, f->bufsiz);
        if (n <= 0)
        {
            if (n == 0)
                f->flags |= _IO_EOF;
            else
                f->flags |= _IO_ERROR;
            return EOF;
        }
        f->len = (unsigned long)n;
    }

    return (unsigned char)f->buf[f->pos++];
}

char *fgets(char *s, int n, FILE *f)
{
    if (n <= 0) return NULL;

    int i = 0;
    while (i < n - 1)
    {
        int c = fgetc(f);
        if (c == EOF)
        {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = (char)c;
        if (c == '\n')
            break;
    }
    s[i] = '\0';
    return s;
}

int ungetc(int c, FILE *f)
{
    if (c == EOF) return EOF;
    f->has_ungetc = 1;
    f->last_ungetc = (unsigned char)c;
    return (unsigned char)c;
}

int feof(FILE *f)
{
    return (f->flags & _IO_EOF) != 0;
}

int ferror(FILE *f)
{
    return (f->flags & _IO_ERROR) != 0;
}

void clearerr(FILE *f)
{
    f->flags &= ~(_IO_EOF | _IO_ERROR);
}
