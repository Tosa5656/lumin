#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: cp <source> <dest>\n");
        return 1;
    }

    int src = open(argv[1], 0);
    if (src < 0)
    {
        printf("cp: %s: no such file\n", argv[1]);
        return 1;
    }

    int dst = open(argv[2], 1);
    if (dst < 0)
    {
        printf("cp: %s: cannot open\n", argv[2]);
        close(src);
        return 1;
    }

    char buf[512];
    int n;
    while ((n = read(src, buf, sizeof(buf))) > 0)
    {
        int off = 0;
        while (off < n)
        {
            int w = write(dst, buf + off, n - off);
            if (w <= 0)
            {
                printf("cp: write error\n");
                close(src);
                close(dst);
                return 1;
            }
            off += w;
        }
    }

    close(src);
    close(dst);
    return 0;
}
