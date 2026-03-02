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
#include "pmm.h"
#include "vmm.h"
#include "spike_interface/spike_utils.h"

#include <stdlib.h>

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char *buf, size_t n) {
    // buf is now an address in user space of the given app's user stack,
    // so we have to transfer it into phisical address (kernel is running in direct mapping).
    assert(current);
    char *pa = (char *)user_va_to_pa((pagetable_t)(current->pagetable), (void *)buf);
    sprint(pa);
    return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
    // 先处理未被free的内存块
    if (current->mem_rib.alloc_list != NULL) {
        pd *alloc_item = current->mem_rib.alloc_list;
        while (alloc_item) {
            uint64 alloc_addr = (uint64)(alloc_item) + sizeof(pd);
            current->mem_rib.free(alloc_addr, &current->mem_rib.free_list, &current->mem_rib.alloc_list);
            alloc_item = current->mem_rib.alloc_list;
        }
    }
    sort_free_list_ascend(&current->mem_rib.free_list, pd_first_fit_cmp);
    merge_free_blocks(&current->mem_rib.free_list, pd_first_fit_cmp);

    pd *free_item = current->mem_rib.free_list;
    if (free_item != NULL) {
        while (free_item) {
            pd *next_item = free_item->next;
            pd *to_free = free_item;
            free_item = free_item->next;
            uint64 user_va = pa_to_user_va((pagetable_t)current->pagetable, (uint64)to_free);
            if (user_va != 0) {
                // 取消映射并释放物理页
                user_vm_unmap((pagetable_t)current->pagetable, user_va, PGSIZE, 1);
            }
            current->mem_rib.free_list = free_item;
        }
    }

    sprint("User exit with code:%d.\n", code);
    // in lab1, PKE considers only one app (one process).
    // therefore, shutdown the system when the app calls exit()
    shutdown(code);
}

//
// maybe, the simplest implementation of malloc in the world ... added @lab2_2
//
uint64 sys_user_allocate_page(int n) {
    if (n <= 0) {
        return (uint64)NULL;
    }
    n = ROUNDUP(n, 8); // 8-byte aligned

    // 检查是否需要跨页分配（请求大小+pd头超过单页可用空间）
    if (n + sizeof(pd) > PGSIZE - sizeof(pd)) {

        // 跨页分配策略：
        // 1. 尝试在现有空闲链表中找到起始位置（优先使用已有页面的空闲空间）
        // 2. 从该位置放置pd头，然后为剩余数据分配新页面

        pd *start_block = NULL;
        uint64 start_va = 0;
        uint64 start_pa = 0;

        // 尝试从空闲链表找一个起始块
        if (current->mem_rib.free_list != NULL) {
            start_block = current->mem_rib.free_list;
            start_pa = (uint64)start_block;
            start_va = pa_to_user_va((pagetable_t)current->pagetable, start_pa);

            // 从空闲链表移除这个块
            current->mem_rib.free_list = start_block->next;
        } else {
            // 没有空闲块，分配一个新页作为起点
            void *pa = alloc_page();
            if (pa == NULL) {
                return (uint64)NULL;
            }
            start_pa = (uint64)pa;
            start_va = current->user_heap_top;
            user_vm_map((pagetable_t)current->pagetable, start_va, PGSIZE, start_pa,
                        prot_to_type(PROT_WRITE | PROT_READ, 1));
            current->user_heap_top += PGSIZE;
            start_block = (pd *)start_pa;
        }

        // 计算需要多少额外的完整页面
        // start_block所在页面可用空间
        uint64 start_page_base = (start_pa / PGSIZE) * PGSIZE;
        uint64 available_in_first_page = PGSIZE - (start_pa - start_page_base);

        // 总共需要的空间
        uint64 total_needed = sizeof(pd) + n;

        // 如果第一个页面不够，分配额外页面
        if (total_needed > available_in_first_page) {
            uint64 remaining = total_needed - available_in_first_page;
            uint64 extra_pages = (remaining + PGSIZE - 1) / PGSIZE;

            for (uint64 i = 0; i < extra_pages; i++) {
                void *pa = alloc_page();
                if (pa == NULL) {
                    return (uint64)NULL;
                }
                uint64 va = current->user_heap_top;
                user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
                            prot_to_type(PROT_WRITE | PROT_READ, 1));
                current->user_heap_top += PGSIZE;
            }
        }

        // 设置pd结构
        start_block->flag = 1;
        start_block->size = n;
        start_block->next = current->mem_rib.alloc_list;
        current->mem_rib.alloc_list = start_block;

        return start_va + sizeof(pd);
    }

    // 单页内分配
    uint64 alloc_pa = current->mem_rib.alloc(n, &current->mem_rib.free_list, &current->mem_rib.alloc_list);
    if (alloc_pa == (uint64)NULL) {
        void *pa = alloc_page();
        if (pa == NULL) {
            return (uint64)NULL;
        }
        uint64 alloc_va = current->user_heap_top;
        user_vm_map((pagetable_t)current->pagetable, alloc_va, PGSIZE, (uint64)pa,
                    prot_to_type(PROT_WRITE | PROT_READ, 1));
        current->user_heap_top += PGSIZE;
        pd *new_free_block = (pd *)pa;
        new_free_block->flag = 0;
        new_free_block->size = PGSIZE - sizeof(pd);
        new_free_block->next = NULL;
        insert_free_block(&current->mem_rib.free_list, new_free_block, pd_first_fit_cmp);

        alloc_pa = current->mem_rib.alloc(n, &current->mem_rib.free_list, &current->mem_rib.alloc_list);
        if (alloc_pa == (uint64)NULL) {
            return (uint64)NULL;
        }
        return pa_to_user_va((pagetable_t)current->pagetable, alloc_pa);
    } else {
        return pa_to_user_va((pagetable_t)current->pagetable, alloc_pa);
    }
}

//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free_page(uint64 va) {
    // user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
    uint64 pa = (uint64)user_va_to_pa((pagetable_t)current->pagetable, (void *)va);
    current->mem_rib.free(pa, &current->mem_rib.free_list, &current->mem_rib.alloc_list);
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
    // added @lab2_2
    case SYS_user_allocate_page:
        return sys_user_allocate_page(a1);
    case SYS_user_free_page:
        return sys_user_free_page(a1);
    default:
        panic("Unknown syscall %ld \n", a0);
    }
}
