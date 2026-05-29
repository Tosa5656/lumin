#ifndef _ASSERT_H
#define _ASSERT_H

#include <stdio.h>
#include <stdlib.h>

#ifndef NDEBUG
#define assert(expr) do { \
    if (!(expr)) { \
        printf("Assertion failed: %s, file %s, line %d\n", #expr, __FILE__, __LINE__); \
        abort(); \
    } \
} while (0)
#else
#define assert(expr) ((void)0)
#endif

#endif
