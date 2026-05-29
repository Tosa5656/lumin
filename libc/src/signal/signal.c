#include <signal.h>
#include <sys/syscall.h>

int kill(int pid, int sig)
{
    return (int)syscall(SYS_kill, (long)pid, (long)sig, 0);
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
    return (int)syscall(SYS_sigaction, (long)sig, (long)act, (long)oldact);
}
