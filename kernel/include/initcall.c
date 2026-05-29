#include "initcall.h"
#include "../drivers/serial/serial.h"

extern initcall_t __start_initcall[];
extern initcall_t __end_initcall[];

void do_initcalls(void)
{
    initcall_t *fn;
    for (fn = __start_initcall; fn < __end_initcall; fn++)
    {
        if (*fn)
        {
            int ret = (*fn)();
            if (ret)
                serial_printf("initcall %p returned %d\n", (void*)*fn, ret);
        }
    }
}
