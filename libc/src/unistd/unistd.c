#include <unistd.h>
#include <sys/syscall.h>

ssize_t read(int fd, void *buf, size_t count)
{
    return (ssize_t)__sysret(__syscall(SYS_read, (long)fd, (long)buf, (long)count));
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return (ssize_t)__sysret(__syscall(SYS_write, (long)fd, (long)buf, (long)count));
}

int open(const char *path, int flags)
{
    return (int)__sysret(__syscall(SYS_open, (long)path, (long)flags, 0));
}

int close(int fd)
{
    return (int)__sysret(__syscall(SYS_close, (long)fd, 0, 0));
}

pid_t fork(void)
{
    return (pid_t)__sysret(__syscall(SYS_fork, 0, 0, 0));
}

pid_t getpid(void)
{
    return (pid_t)__sysret(__syscall(SYS_getpid, 0, 0, 0));
}

int sched_yield(void)
{
    return (int)__sysret(__syscall(SYS_yield, 0, 0, 0));
}

void *sbrk(long increment)
{
    long old = __syscall(SYS_brk, 0, 0, 0);
    if (increment == 0)
        return (void *)old;
    long new = __syscall(SYS_brk, old + increment, 0, 0);
    if (new == -1)
        return (void *)-1;
    return (void *)old;
}

int getcwd(char *buf, unsigned long size)
{
    return (int)__sysret(__syscall(SYS_getcwd, (long)buf, (long)size, 0));
}

int chdir(const char *path)
{
    return (int)__sysret(__syscall(SYS_chdir, (long)path, 0, 0));
}

int pipe(int pipefd[2])
{
    return (int)__sysret(__syscall(SYS_pipe, (long)pipefd, 0, 0));
}

int dup(int oldfd)
{
    return (int)__sysret(__syscall(SYS_dup, (long)oldfd, 0, 0));
}

int dup2(int oldfd, int newfd)
{
    return (int)__sysret(__syscall(SYS_dup2, (long)oldfd, (long)newfd, 0));
}

void _exit(int code)
{
    __syscall(SYS_exit, code, 0, 0);
    while (1) {}
}

unsigned int sleep(unsigned int seconds)
{
    unsigned long target = seconds * 1000000UL;
    return usleep(target);
}

int usleep(unsigned long usec)
{
    struct {
        unsigned long tv_sec;
        unsigned long tv_nsec;
    } ts;
    ts.tv_sec  = usec / 1000000UL;
    ts.tv_nsec = (usec % 1000000UL) * 1000UL;
    return (int)__sysret(__syscall(SYS_nanosleep, (long)&ts, 0, 0));
}

int execve(const char *path, char **argv, char **envp)
{
    (void)envp;
    int argc = 0;
    if (argv)
        while (argv[argc]) argc++;
    return (int)__sysret(__syscall(SYS_exec, (long)path, (long)argc, (long)argv));
}

int execv(const char *path, char **argv)
{
    return execve(path, argv, NULL);
}

int rmdir(const char *path)
{
    return (int)__sysret(__syscall(SYS_rmdir, (long)path, 0, 0));
}

int unlink(const char *path)
{
    return (int)__sysret(__syscall(SYS_unlink, (long)path, 0, 0));
}

long lseek(int fd, long offset, int whence)
{
    return (long)__sysret(__syscall(SYS_lseek, (long)fd, (long)offset, (long)whence));
}

int isatty(int fd)
{
    return fd >= 0 && fd <= 2;
}

int shutdown(void)
{
    __syscall(SYS_shutdown, 0, 0, 0);
    return 0;
}
