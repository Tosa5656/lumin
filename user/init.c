#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("Hello from userspace! (pid init)\n");
    printf("Userspace is working!\n");

    char *p = (char *)malloc(32);
    if (p)
    {
        strcpy(p, "malloc via brk works!");
        printf("%s\n", p);
    }
    else
    {
        printf("malloc failed\n");
    }

    void *q = malloc(4096);
    printf("malloc(32)=%p malloc(4096)=%p\n", p, q);

    exit(0);
    return 0;
}
