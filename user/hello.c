#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("Hello from exec'ed program!\n");
    printf("Userspace exec works!\n");
    exit(0);
    return 0;
}