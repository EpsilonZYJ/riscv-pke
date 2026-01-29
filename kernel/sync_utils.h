#ifndef _SYNC_UTILS_H_
#define _SYNC_UTILS_H_

static inline void sync_barrier(volatile int *counter, int all) {
    int local;

    asm volatile("amoadd.w %0, %2, (%1)\n"
                 : "=r"(local)
                 : "r"(counter), "r"(1)
                 : "memory");

    if (local + 1 < all) {
        do {
            asm volatile("lw %0, (%1)\n" : "=r"(local) : "r"(counter) : "memory");
        } while (local < all);
    }
}

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

#endif