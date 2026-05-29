#include <signal.h>
#include <sys/syscall.h>

int kill(int pid, int sig)
{
    return (int)__sysret(__syscall(SYS_kill, (long)pid, (long)sig, 0));
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
    return (int)__sysret(__syscall(SYS_sigaction, (long)sig, (long)act, (long)oldact));
}

void (*signal(int sig, void (*handler)(int)))(int)
{
    struct sigaction old;
    struct sigaction new;
    new.sa_handler = handler;
    new.sa_mask = 0;
    new.sa_flags = 0;
    if (sigaction(sig, &new, &old) < 0)
        return SIG_ERR;
    return old.sa_handler;
}
