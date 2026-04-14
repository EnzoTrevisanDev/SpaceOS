#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

/*
 * Spinlock simples para SMP — usa operacao atomica test-and-set.
 * Em single-core (BSP apenas), degrada a no-op (sem contencao).
 */
typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spin_lock(spinlock_t *l) {
    while (__sync_lock_test_and_set(&l->locked, 1)) {
        /* busy-wait com PAUSE para reduzir contencao de barramento */
        while (l->locked)
            __asm__ volatile ("pause");
    }
}

static inline void spin_unlock(spinlock_t *l) {
    __sync_lock_release(&l->locked);
}

static inline int spin_trylock(spinlock_t *l) {
    return !__sync_lock_test_and_set(&l->locked, 1);
}

#endif
