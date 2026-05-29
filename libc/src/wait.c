#include <sys/wait.h>
#include <sys/syscall.h>
#include <stddef.h>

pid_t wait(int *status)
{
    int code;
    while ((code = (int)__syscall(SYS_waitpid, -1, 0, 0)) == -2);
    if (status)
        *status = code;
    return 1;
}
