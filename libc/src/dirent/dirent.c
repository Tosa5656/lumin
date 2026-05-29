#include <dirent.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>

DIR *opendir(const char *path)
{
    DIR *dir = (DIR *)malloc(sizeof(DIR) + strlen(path) + 1);
    if (!dir) return NULL;

    char *p = (char *)(dir + 1);
    strcpy(p, path);
    dir->path = p;
    dir->pos = 0;
    dir->current.ino = 0;
    dir->current.name[0] = '\0';
    dir->current.type = 0;
    dir->current.size = 0;
    dir->has_next = 1;
    return dir;
}

struct vfs_dentry *readdir(DIR *dir)
{
    if (!dir || !dir->has_next)
        return NULL;

    int r = __readdir(dir->path, (unsigned int)dir->pos, &dir->current);
    if (r != 0)
    {
        dir->has_next = 0;
        return NULL;
    }

    dir->pos++;
    return &dir->current;
}

int closedir(DIR *dir)
{
    if (dir)
        free(dir);
    return 0;
}
