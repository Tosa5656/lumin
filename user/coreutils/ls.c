#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/syscall.h>

#define MAX_PATH_LEN 256

int main(int argc, char **argv)
{
    const char *path = "/";
    char cwd_buffer[MAX_PATH_LEN];
    int res;

    if (argc > 1)
        path = argv[1];
    else
        res = getcwd(cwd_buffer, MAX_PATH_LEN);
        path = cwd_buffer;

    struct vfs_dentry entry;
    for (unsigned int i = 0; readdir(path, i, &entry) == 0; i++)
        printf("%s  ", entry.name);
    printf("\n");
    return 0;
}
