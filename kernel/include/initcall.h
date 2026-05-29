#ifndef INITCALL_H
#define INITCALL_H

typedef int (*initcall_t)(void);

#define __init_call __attribute__((__section__(".initcall")))

#define pure_initcall(fn)   static initcall_t __initcall_##fn __init_call = fn
#define core_initcall(fn)   static initcall_t __initcall_##fn __init_call = fn
#define device_initcall(fn) static initcall_t __initcall_##fn __init_call = fn

void do_initcalls(void);

#endif