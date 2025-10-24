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
#include "elf.h"

#include "spike_interface/spike_utils.h"

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char *buf, size_t n) {
    sprint(buf);
    return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
    sprint("User exit with code:%d.\n", code);
    // in lab1, PKE considers only one app (one process).
    // therefore, shutdown the system when the app calls exit()
    shutdown(code);
}

/**
 * @brief 处理打印调用栈的系统调用
 *
 * @version 0.1
 * @author EpsilonZYJ (yujie.zhou05@outlook.com)
 * @date 2025-10-20
 * @copyright Copyright (c) 2025
 */
ssize_t sys_user_print_backtrace(int depth) {
    if (depth <= 0) {
        return EINVAL;
    }

    // 当前的栈帧指针
    uint64 *cur_fp;
    // 当前的返回函数地址
    uint64 cur_ra;
    // 当前栈帧的基址指针
    uint64 *cur_sb = (uint64 *)current->trapframe->regs.sp;
    // 先从print_backtrace中跳出
    cur_fp = (uint64 *)((uint64)cur_sb + 32); // 到上一个函数的调用栈栈底
    cur_sb = (uint64 *)((uint64)cur_fp + 8);  // 上一个函数调用栈的基址
    cur_ra = *cur_sb;                         // 到调用print_backtrace的返回地址

#ifdef SYS_USER_PRINT_BACKTRACE_DEBUG
    sprint("=====================================\n");
    for (int i = 64; i >= -64; i--) {
        if (!i)
            sprint("-> ");
        else
            sprint("   ");
        sprint("Stack dump [0x%016lx]: 0x%016lx\n", current->trapframe->regs.sp + 32 + i * sizeof(uint64), *(uint64 *)(current->trapframe->regs.sp + 32 + i * sizeof(uint64)));
    }
    sprint("=====================================\n");
#endif

    while (depth > 0) {
        const char *func_name = elf_find_symbol_by_addr(cur_ra);
        if (func_name == NULL) {
            sprint("  [0x%016lx]j <unknown>\n", cur_ra);
            return ENXIO;
        } else if (strcmp(func_name, "main") == 0) {
            sprint("%s\n", func_name);
            return EINVAL;
        } else {
            sprint("%s\n", func_name);
            depth--;
            cur_sb = (uint64 *)((uint64)(*cur_fp) - 8); // 到调用函数的栈帧基址
            cur_ra = *cur_sb;                           // 到调用函数的返回地址
            cur_fp = (uint64 *)((uint64)cur_sb - 8);    // 更新栈帧指针
        }
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
    case SYS_user_print_backtrace:
        return sys_user_print_backtrace(a1);
    default:
        panic("Unknown syscall %ld \n", a0);
    }
}
