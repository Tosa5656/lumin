#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/syscall.h>

int main(int argc, char **argv)
{
    const char *path = "/";
    if (argc > 1)
        path = argv[1];

    struct vfs_dentry entry;
    for (unsigned int i = 0; readdir(path, i, &entry) == 0; i++)
        printf("%s  ", entry.name);
    printf("\n");
    return 0;
}
