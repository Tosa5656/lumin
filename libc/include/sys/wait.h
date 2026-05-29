#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#define WIFEXITED(status)   ((status) >= 0 && (status) < 0x100)
#define WEXITSTATUS(status) ((status) & 0xFF)
#define WIFSIGNALED(status) ((status) >= 0x100 && (status) < 0x200)
#define WTERMSIG(status)    ((status) & 0xFF)
#define WIFSTOPPED(status)  ((status) >= 0x200)
#define WSTOPSIG(status)    ((status) & 0xFF)

#endif
