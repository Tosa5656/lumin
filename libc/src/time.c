#include <time.h>
#include <sys/syscall.h>

int clock_gettime(int clk_id, struct timespec *tp)
{
    (void)clk_id;
    return (int)__sysret(__syscall(SYS_clock_gettime, (long)tp, 0, 0));
}
