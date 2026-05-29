#include <sys/syscall.h>
#include <unistd.h>

int main(void)
{
    for (;;)
    {
        char *argv[] = { "shell", NULL };
        int pid = spawn("/system/bin/shell.elf", 1, argv);
        if (pid < 0)
            for (;;);

        int code;
        while ((code = (int)syscall(SYS_waitpid, (long)pid, 0, 0)) == -2);
    }
    return 0;
}
