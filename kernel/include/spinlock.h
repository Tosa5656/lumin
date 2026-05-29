#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spin_init(spinlock_t *lock)
{
    lock->locked = 0;
    __asm__ volatile("" : : : "memory");
}

static inline void spin_lock(spinlock_t *lock)
{
    uint32_t tmp;
    __asm__ volatile(
        "1: xorl %0, %0\n"
        "   lock btsl $0, %1\n"
        "   jc 2f\n"
        "   mfence\n"
        "   jmp 3f\n"
        "2: pause\n"
        "   cmpl $0, %1\n"
        "   jne 2b\n"
        "   jmp 1b\n"
        "3:"
        : "=&r"(tmp), "+m"(lock->locked)
        :
        : "cc", "memory");
}

static inline void spin_unlock(spinlock_t *lock)
{
    __asm__ volatile(
        "mfence\n"
        "movl $0, %0\n"
        : "+m"(lock->locked)
        :
        : "memory");
}

static inline int spin_trylock(spinlock_t *lock)
{
    uint32_t old;
    __asm__ volatile(
        "movl $1, %0\n"
        "xchgl %0, %1\n"
        : "=&r"(old), "+m"(lock->locked)
        :
        : "memory");
    return old == 0;
}

static inline unsigned long irq_save(void)
{
    unsigned long flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(unsigned long flags)
{
    __asm__ volatile("pushq %0; popfq" : : "r"(flags) : "memory", "cc");
}

static inline void spin_lock_irqsave(spinlock_t *lock, unsigned long *flags)
{
    *flags = irq_save();
    spin_lock(lock);
}

static inline void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags)
{
    spin_unlock(lock);
    irq_restore(flags);
}

#endif
