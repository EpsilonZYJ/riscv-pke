//
// Created by 周煜杰 on 2026/1/29.
//

#ifndef _LOCK_H_
#define _LOCK_H_

static inline void spin_lock(volatile int *lock_ptr) {
    int one = 1;
    int old_val;
    do {
        asm volatile(
            "amoswap.w.aq %0, %2, (%1)"
            : "=r"(old_val)
            : "r"(lock_ptr), "r"(one)
            : "memory");
    } while (old_val != 0);
}

static inline void spin_unlock(volatile int *lock_ptr) {
    asm volatile(
        "amoswap.w.rl x0, x0, (%0)"
        :
        : "r"(lock_ptr)
        : "memory");
}

#endif // _LOCK_H_
