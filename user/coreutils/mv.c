#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: mv <source> <dest>\n");
        return 1;
    }

    int src = open(argv[1], 0);
    if (src < 0)
    {
        printf("mv: %s: no such file\n", argv[1]);
        return 1;
    }

    char *buf = malloc(65536);
    if (!buf)
    {
        printf("mv: out of memory\n");
        close(src);
        return 1;
    }

    int total = 0;
    int n;
    while ((n = read(src, buf + total, 65536 - total)) > 0)
        total += n;
    close(src);

    int dst = open(argv[2], 1);
    if (dst < 0)
    {
        printf("mv: %s: cannot create\n", argv[2]);
        free(buf);
        return 1;
    }

    int off = 0;
    while (off < total)
    {
        int w = write(dst, buf + off, total - off);
        if (w <= 0)
        {
            printf("mv: write error\n");
            close(dst);
            free(buf);
            return 1;
        }
        off += w;
    }
    close(dst);
    free(buf);

    if (unlink(argv[1]) < 0)
        printf("mv: %s: cannot remove\n", argv[1]);

    return 0;
}
