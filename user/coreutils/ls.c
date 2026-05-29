#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/syscall.h>

#define MAX_PATH_LEN 512

int main(int argc, char **argv)
{
    const char *path = "/";
    char cwd_buffer[MAX_PATH_LEN];
    int res;

    if (argc >= 2)
    {
        path = argv[1];
    }
    else
    {
        res = getcwd(cwd_buffer, MAX_PATH_LEN);
        if (res >= 0)
        {
            path = cwd_buffer;
        }
        else
        {
            path = "/";
        }
    }

    struct vfs_dentry entry;

    for (unsigned int i = 0; __readdir(path, i, &entry) == 0; i++)
    {
        printf("%s  ", entry.name);
    }
    printf("\n");

    return 0;
}