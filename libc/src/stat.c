#include <sys/stat.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stddef.h>

int stat(const char *path, struct stat *buf)
{
    return (int)__sysret(__syscall(SYS_stat, (long)path, (long)buf, 0));
}

int fstat(int fd, struct stat *buf)
{
    (void)fd;
    (void)buf;
    errno = ENOSYS;
    return -1;
}

int mkdir(const char *path, int mode)
{
    (void)mode;
    return (int)__sysret(__syscall(SYS_mkdir, (long)path, 0, 0));
}

int chmod(const char *path, int mode)
{
    (void)path;
    (void)mode;
    errno = ENOSYS;
    return -1;
}
