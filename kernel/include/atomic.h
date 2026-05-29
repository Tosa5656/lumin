#ifndef ATOMIC_H
#define ATOMIC_H

#include <stdint.h>

typedef struct { volatile int64_t counter; } atomic64_t;
typedef struct { volatile int32_t counter; } atomic_t;

#define ATOMIC_INIT(i) { (i) }

static inline int atomic_read(const atomic_t *v)
{
    return v->counter;
}

static inline void atomic_set(atomic_t *v, int i)
{
    v->counter = i;
}

static inline void atomic_add(atomic_t *v, int i)
{
    __asm__ volatile("lock addl %1, %0"
                     : "+m" (v->counter)
                     : "ir" (i)
                     : "memory");
}

static inline void atomic_sub(atomic_t *v, int i)
{
    __asm__ volatile("lock subl %1, %0"
                     : "+m" (v->counter)
                     : "ir" (i)
                     : "memory");
}

static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
    int ret;
    __asm__ volatile("lock cmpxchgl %2, %1"
                     : "=a"(ret), "+m"(v->counter)
                     : "r"(new), "0"(old)
                     : "memory");
    return ret;
}

static inline int atomic_xchg(atomic_t *v, int new)
{
    int ret;
    __asm__ volatile("xchgl %0, %1"
                     : "=r"(ret), "+m"(v->counter)
                     : "0"(new)
                     : "memory");
    return ret;
}

static inline int atomic_inc_return(atomic_t *v)
{
    int ret = 1;
    __asm__ volatile("lock xaddl %0, %1"
                     : "+r"(ret), "+m"(v->counter)
                     : : "memory");
    return ret + 1;
}

#endif
