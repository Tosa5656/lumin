#ifndef _UNISTD_H
#define _UNISTD_H

#include <sys/types.h>

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int open(const char *path, int flags);
int close(int fd);
pid_t fork(void);
pid_t getpid(void);
int sched_yield(void);
void *sbrk(long increment);
int getcwd(char *buf, unsigned long size);
int chdir(const char *path);
int pipe(int pipefd[2]);
int dup(int oldfd);
int dup2(int oldfd, int newfd);

void _exit(int code);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned long usec);
int execve(const char *path, char **argv, char **envp);
int execv(const char *path, char **argv);
int rmdir(const char *path);
int unlink(const char *path);
long lseek(int fd, long offset, int whence);
int isatty(int fd);
int shutdown(void);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
