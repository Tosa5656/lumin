#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("Hello from userspace! (pid init)\n");
    printf("Userspace is working!\n");
    exit(0);
    return 0;
}
