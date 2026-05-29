#ifndef PERCPU_H
#define PERCPU_H

#include <stdint.h>

#define __percpu __attribute__((__section__(".percpu")))

#define DECLARE_PER_CPU(type, name) \
    extern __percpu type percpu_##name

#define DEFINE_PER_CPU(type, name) \
    __percpu type percpu_##name

#define per_cpu(var, cpu) \
    (*({ extern __percpu typeof(percpu_##var) percpu_##var[]; \
         (typeof(percpu_##var)*)((uint64_t)percpu_##var + (cpu) * 0); }))

#define this_cpu_ptr(var) \
    ({ extern __percpu typeof(percpu_##var) percpu_##var; \
       uint64_t __ptr; \
       __asm__("movq %%gs:0, %0" : "=r"(__ptr)); \
       (typeof(percpu_##var)*)(__ptr + (uint64_t)&percpu_##var); })

#define this_cpu_read(var) \
    ({ extern __percpu typeof(percpu_##var) percpu_##var; \
       typeof(percpu_##var) __val; \
       __asm__("movq %%gs:%1, %0" : "=r"(__val) \
               : "m"(percpu_##var)); \
       __val; })

#define this_cpu_write(var, val) \
    do { \
        extern __percpu typeof(percpu_##var) percpu_##var; \
        __asm__("movq %0, %%gs:%1" : : "r"((uint64_t)(val)), \
                "m"(percpu_##var)); \
    } while (0)

#define MSR_GS_BASE 0xC0000101
#define MSR_FS_BASE 0xC0000100

static inline void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi) : "memory");
}

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

#endif
