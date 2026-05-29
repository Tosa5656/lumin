#include <unistd.h>
#include <sys/syscall.h>

ssize_t read(int fd, void *buf, size_t count)
{
    return (ssize_t)syscall(SYS_read, (long)fd, (long)buf, (long)count);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return (ssize_t)syscall(SYS_write, (long)fd, (long)buf, (long)count);
}

int open(const char *path, int flags)
{
    return (int)syscall(SYS_open, (long)path, (long)flags, 0);
}

int close(int fd)
{
    return (int)syscall(SYS_close, (long)fd, 0, 0);
}

pid_t fork(void)
{
    return (pid_t)syscall(SYS_fork, 0, 0, 0);
}

pid_t getpid(void)
{
    return (pid_t)syscall(SYS_getpid, 0, 0, 0);
}

int sched_yield(void)
{
    return (int)syscall(SYS_yield, 0, 0, 0);
}

void *sbrk(long increment)
{
    long old = syscall(SYS_brk, 0, 0, 0);
    if (increment == 0)
        return (void *)old;
    long new = syscall(SYS_brk, old + increment, 0, 0);
    if (new == -1)
        return (void *)-1;
    return (void *)old;
}

int getcwd(char *buf, unsigned long size)
{
    return (int)syscall(SYS_getcwd, (long)buf, (long)size, 0);
}

int chdir(const char *path)
{
    return (int)syscall(SYS_chdir, (long)path, 0, 0);
}

int pipe(int pipefd[2])
{
    return (int)syscall(SYS_pipe, (long)pipefd, 0, 0);
}

int dup(int oldfd)
{
    return (int)syscall(SYS_dup, (long)oldfd, 0, 0);
}

int dup2(int oldfd, int newfd)
{
    return (int)syscall(SYS_dup2, (long)oldfd, (long)newfd, 0);
}