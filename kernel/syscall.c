/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"

#include "spike_interface/spike_utils.h"
#include "kernel/sync_utils.h"

static volatile int exit_counter = 0;

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char *buf, size_t n) {
    uint64 hartid = read_tp();
    assert(hartid < NCPU);
    sprint("hartid = %d: %s", hartid, buf);
    return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
    uint64 hartid = read_tp();
    assert(hartid < NCPU);
    if (hartid == 0) {
        sprint("hartid = %d: User exit with code:%d.\n", hartid, code);
        sync_barrier(&exit_counter, NCPU);
        sprint("hartid = %d: shutdown with code:%d.\n", hartid, code);
        // in lab1, PKE considers only one app (one process).
        // therefore, shutdown the system when the app calls exit()
        shutdown(code);
    } else {
        sprint("hartid = %d: User exit with code:%d.\n", hartid, code);
        sync_barrier(&exit_counter, NCPU);
    }
    return 0;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    switch (a0) {
    case SYS_user_print:
        return sys_user_print((const char *)a1, a2);
    case SYS_user_exit:
        return sys_user_exit(a1);
    default:
        panic("Unknown syscall %ld \n", a0);
    }
}
