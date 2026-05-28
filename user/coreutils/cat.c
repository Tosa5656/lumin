#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: cat <file>\n");
        return 1;
    }

    int fd = open(argv[1], 0);
    if (fd < 0)
    {
        printf("cat: %s: not found\n", argv[1]);
        return 1;
    }

    char buf[512];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(1, buf, n);

    close(fd);
    return 0;
}
