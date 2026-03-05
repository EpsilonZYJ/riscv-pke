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
#include "sched.h"
#include "proc_file.h"

#include "spike_interface/spike_utils.h"
#include "kernel/semaphore.h"
#include "semaphore.h"

#include "elf.h"

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
    sprint("User exit with code:%d.\n", code);
    // reclaim the current process, and reschedule. added @lab3_1
    process *tmp = wake_from_block_queue(&block_queue_head, current);
    free_process(current);

    // 先处理未被free的内存块
    if (current->user_heap.mem_rib.alloc_list != NULL) {
        pd *alloc_item = current->user_heap.mem_rib.alloc_list;
        while (alloc_item) {
            uint64 alloc_addr = (uint64)(alloc_item);
            current->user_heap.mem_rib.free(alloc_addr, &current->user_heap.mem_rib.free_list, &current->user_heap.mem_rib.alloc_list);
            alloc_item = current->user_heap.mem_rib.alloc_list;
        }
    }
    sort_free_list_ascend(&current->user_heap.mem_rib.free_list, PD_CMP_FUNC);
    merge_free_blocks(&current->user_heap.mem_rib.free_list, PD_CMP_FUNC);

    pd *free_item = current->user_heap.mem_rib.free_list;
    if (free_item != NULL) {
        while (free_item) {
            pd *next_item = get_next(free_item);
            pd *to_free = free_item;
            free_item = get_next(free_item);
            if (get_size(to_free) + sizeof(pd) <= PGSIZE) {
                // 取消映射并释放物理页
                user_vm_unmap((pagetable_t)current->pagetable, (uint64)to_free, PGSIZE, 1);
            }
            current->user_heap.mem_rib.free_list = free_item;
        }
    }

    // FIXME: 可以使用heap_top和heap_bottom来优化释放过程，直接释放整个堆空间，而不需要逐块释放。
    for (uint64 heap_va = current->user_heap.heap_bottom; heap_va < current->user_heap.heap_top; heap_va += PGSIZE) {
        pte_t *pte = page_walk((pagetable_t)current->pagetable, heap_va, 0);
        if (pte && (*pte & PTE_V)) {
            // 页面已映射，需要释放
            uint64 pa = PTE2PA(*pte);
            // 取消映射并释放物理页
            user_vm_unmap((pagetable_t)current->pagetable, heap_va, PGSIZE, 1);
        }
    }

    if (tmp == NULL) {
        schedule();
        return 0;
    }
    current = tmp;
    current->status = READY;
    insert_to_ready_queue(current);
    schedule();
    return 0;
}

// //
// // implementation of malloc in the world ... added @lab2_2
// //
// uint64 sys_user_allocate_mem(int n) {
//     if (n <= 0) {
//         return (uint64)NULL;
//     }
//     n = ROUNDUP(n, 8); // 8-byte aligned
//     if (n + sizeof(pd) <= PGSIZE) {
//         // 单页内分配
//         uint64 alloc_pa = current->user_heap.mem_rib.alloc(n, &current->user_heap.mem_rib.free_list, &current->user_heap.mem_rib.alloc_list);
//         if (alloc_pa == (uint64)NULL) {
//             void *pa = alloc_page();
//             if (pa == NULL) {
//                 return (uint64)NULL;
//             }
//             current->mapped_info[HEAP_SEGMENT].npages++;
//             uint64 alloc_va = current->user_heap.heap_top;
//             user_vm_map((pagetable_t)current->pagetable, alloc_va, PGSIZE, (uint64)pa,
//                         prot_to_type(PROT_WRITE | PROT_READ, 1));
//             current->user_heap.heap_top += PGSIZE;
//             pd *new_free_block = (pd *)pa;
//             new_free_block->flag = 0;
//             new_free_block->size = PGSIZE - sizeof(pd);
//             new_free_block->next = NULL;
//             insert_free_block(&current->user_heap.mem_rib.free_list, new_free_block, PD_CMP_FUNC);
//
//             alloc_pa = current->user_heap.mem_rib.alloc(n, &current->user_heap.mem_rib.free_list, &current->user_heap.mem_rib.alloc_list);
//             if (alloc_pa == (uint64)NULL) {
//                 return (uint64)NULL;
//             }
//             return pa_to_user_va((pagetable_t)current->pagetable, alloc_pa);
//         } else {
//             return pa_to_user_va((pagetable_t)current->pagetable, alloc_pa);
//         }
//     }
//     // 检查是否需要跨页分配（请求大小+pd头超过单页可用空间）
//     else {
//         // 跨页分配策略：
//         // 1. 尝试在现有空闲链表中找到起始位置（优先使用已有页面的空闲空间）
//         // 2. 从该位置放置pd头，然后为剩余数据分配新页面
//
//         pd *start_block = NULL;
//         uint64 start_va = 0;
//         uint64 start_pa = 0;
//
//         // 尝试从空闲链表找一个起始块
//         if (current->user_heap.mem_rib.free_list != NULL) {
//             start_block = current->user_heap.mem_rib.free_list;
//             start_pa = (uint64)start_block;
//             start_va = pa_to_user_va((pagetable_t)current->pagetable, start_pa);
//
//             // 从空闲链表移除这个块
//             current->user_heap.mem_rib.free_list = start_block->next;
//         } else {
//             // 没有空闲块，分配一个新页作为起点
//             void *pa = alloc_page();
//             if (pa == NULL) {
//                 return (uint64)NULL;
//             }
//             current->mapped_info[HEAP_SEGMENT].npages++;
//             start_block = (pd *)start_pa;
//             start_pa = (uint64)pa + sizeof(pd);
//             start_va = current->user_heap.heap_top;
//             user_vm_map((pagetable_t)current->pagetable, start_va, PGSIZE - sizeof(pd), start_pa,
//                         prot_to_type(PROT_WRITE | PROT_READ, 1));
//             current->user_heap.heap_top += PGSIZE;
//         }
//
//         // 计算需要多少额外的完整页面
//         // start_block所在页面可用空间
//         uint64 start_page_base = (start_pa / PGSIZE) * PGSIZE;
//         uint64 available_in_first_page = PGSIZE - (start_pa - start_page_base);
//
//         // 总共需要的空间
//         uint64 total_needed = sizeof(pd) + n;
//         uint64 extra_pages = 0;
//
//         // 如果第一个页面不够，分配额外页面
//         if (total_needed > available_in_first_page) {
//             uint64 remaining = total_needed - available_in_first_page;
//             extra_pages = (remaining + PGSIZE - 1) / PGSIZE;
//
//             for (uint64 i = 0; i < extra_pages; i++) {
//                 void *pa = alloc_page();
//                 if (pa == NULL) {
//                     return (uint64)NULL;
//                 }
//                 current->mapped_info[HEAP_SEGMENT].npages++;
//                 uint64 va = current->user_heap.heap_top;
//                 user_vm_map((pagetable_t)current->pagetable, va, PGSIZE, (uint64)pa,
//                             prot_to_type(PROT_WRITE | PROT_READ, 1));
//                 current->user_heap.heap_top += PGSIZE;
//             }
//         }
//
//         // 设置pd结构
//         start_block->flag = 1;
//         start_block->size = available_in_first_page + extra_pages * PGSIZE - sizeof(pd);
//         start_block->next = current->user_heap.mem_rib.alloc_list;
//         current->user_heap.mem_rib.alloc_list = start_block;
//
//         return start_va + sizeof(pd);
//     }
// }

//
// implementation of malloc in the world ... added @lab2_2
//
uint64 sys_user_allocate_mem(int n) {
    if (n <= 0) {
        return (uint64)NULL;
    }
    n = ROUNDUP(n, 8); // 8-byte aligned
#ifdef MEM_DEBUG
    sprint("sizeof(pd): %d, request size: %d\n", sizeof(pd), n);
#endif

    if (n + sizeof(pd) <= PGSIZE) {
        // 单页内分配
        uint64 alloc_va = current->user_heap.mem_rib.alloc(n, &current->user_heap.mem_rib.free_list, &current->user_heap.mem_rib.alloc_list);
        if (alloc_va != (uint64)NULL) {
            return alloc_va + sizeof(pd);
        } else {
            void *pa = alloc_page();
            if (pa == NULL) {
                return (uint64)NULL;
            }
            current->mapped_info[HEAP_SEGMENT].npages++;
            uint64 alloc_va = current->user_heap.heap_top;
            user_vm_map((pagetable_t)current->pagetable, alloc_va, PGSIZE, (uint64)pa,
                        prot_to_type(PROT_WRITE | PROT_READ, 1));
            current->user_heap.heap_top += PGSIZE;
            // pd *new_free_block = (pd *)user_va_to_pa(current->pagetable, (void *)alloc_va);
            pd *new_free_block = (void *)alloc_va;
#ifdef MEM_DEBUG
            sprint("new free block at va: %lx, pa: %lx\n", alloc_va, user_va_to_pa(current->pagetable, (void *)alloc_va));
#endif
            set_flag(new_free_block, 0);
            set_size(new_free_block, PGSIZE - sizeof(pd));
            set_next(new_free_block, NULL);
            insert_free_block(&current->user_heap.mem_rib.free_list, new_free_block, PD_CMP_FUNC);
            alloc_va = current->user_heap.mem_rib.alloc(n, &current->user_heap.mem_rib.free_list, &current->user_heap.mem_rib.alloc_list);
            if (alloc_va != (uint64)NULL) {
                return alloc_va + sizeof(pd);
            } else {
                return (uint64)NULL;
            }
        }
    }
    // 检查是否需要跨页分配（请求大小+pd头超过单页可用空间）
    else {
        int original_n = n;
        // 跨页分配策略：
        // 尝试在现有空闲链表中找到是否有大块
        uint64 alloc_va = current->user_heap.mem_rib.alloc(n, &current->user_heap.mem_rib.free_list, &current->user_heap.mem_rib.alloc_list);
        if (alloc_va != (uint64)NULL) {
            return alloc_va + sizeof(pd);
        }

        // 没有大块，从当前虚拟地址最大的位置开始分配连续的页
        pd *highest_block = NULL;
        for (pd *block = current->user_heap.mem_rib.free_list; block != NULL; block = get_next(block)) {
            if (highest_block == NULL || (uint64)block > (uint64)highest_block) {
                highest_block = block;
            }
        }
        uint64 start_va = 0;
        // 如果highest_block不为NULL，说明有空闲块，分配在其上方
        if (highest_block != NULL) {
            start_va = (uint64)highest_block;
            // 从空闲链表移除这个块
            remove_from_pd_list(&current->user_heap.mem_rib.free_list, highest_block);
            n = n - get_size(highest_block);
        } else {
            // 先分配一页
            void *pa = alloc_page();
            if (pa == NULL) {
                return (uint64)NULL;
            }
            current->mapped_info[HEAP_SEGMENT].npages++;
            uint64 alloc_va = current->user_heap.heap_top;
            user_vm_map((pagetable_t)current->pagetable, alloc_va, PGSIZE, (uint64)pa,
                        prot_to_type(PROT_WRITE | PROT_READ, 1));
            current->user_heap.heap_top += PGSIZE;
            start_va = alloc_va;
            set_size((pd *)start_va, PGSIZE - sizeof(pd));
            n = n - (PGSIZE - sizeof(pd));
            start_va = alloc_va;
        }
        int i;
        for (i = 0; i * PGSIZE < n; i++) {
            void *pa = alloc_page();
            if (pa == NULL) {
                pd *new_alloc_block = (pd *)start_va;
                set_flag(new_alloc_block, 0);
                set_size(new_alloc_block, (i - 1) * PGSIZE + get_size((pd *)start_va));
                insert_free_block(&current->user_heap.mem_rib.free_list, new_alloc_block, PD_CMP_FUNC);

                return (uint64)NULL;
            }
            current->mapped_info[HEAP_SEGMENT].npages++;
            uint64 alloc_va = current->user_heap.heap_top;
            user_vm_map((pagetable_t)current->pagetable, alloc_va, PGSIZE, (uint64)pa,
                        prot_to_type(PROT_WRITE | PROT_READ, 1));
            current->user_heap.heap_top += PGSIZE;
        }
        if (i * PGSIZE != n) {
            // 最后剩余的部分再分配一页
            void *pa = alloc_page();
            if (pa == NULL) {
                pd *new_alloc_block = (pd *)start_va;
                set_flag(new_alloc_block, 0);
                set_size(new_alloc_block, (i - 1) * PGSIZE + get_size((pd *)start_va));
                insert_free_block(&current->user_heap.mem_rib.free_list, new_alloc_block, PD_CMP_FUNC);

                return (uint64)NULL;
            }
            current->mapped_info[HEAP_SEGMENT].npages++;
            uint64 alloc_va = current->user_heap.heap_top;
            user_vm_map((pagetable_t)current->pagetable, alloc_va, PGSIZE, (uint64)pa,
                        prot_to_type(PROT_WRITE | PROT_READ, 1));
            current->user_heap.heap_top += PGSIZE;
            uint64 free_va = alloc_va + n - i * PGSIZE;
            pd *new_free_block = (pd *)free_va;
            set_flag(new_free_block, 0);
            set_size(new_free_block, PGSIZE - (n - i * PGSIZE) - sizeof(pd));
            set_next(new_free_block, NULL);
            insert_free_block(&current->user_heap.mem_rib.free_list, new_free_block, PD_CMP_FUNC);
        }
        pd *new_alloc_block = (pd *)start_va;
        set_flag(new_alloc_block, 1);
        set_size(new_alloc_block, original_n);
        insert_alloc_block(&current->user_heap.mem_rib.alloc_list, new_alloc_block);
        return start_va + sizeof(pd);
    }
}

//
// maybe, the simplest implementation of malloc in the world ... added @lab2_2
//
inline uint64 sys_user_allocate_page() {
    return sys_user_allocate_mem(PGSIZE - sizeof(pd));
}

// //
// // reclaim a page, indicated by "va". added @lab2_2
// //
// uint64 sys_user_free_mem(uint64 va) {
//     // user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
//     uint64 pa = (uint64)user_va_to_pa((pagetable_t)current->pagetable, (void *)va);
//     current->user_heap.mem_rib.free(pa, &current->user_heap.mem_rib.free_list, &current->user_heap.mem_rib.alloc_list);
//     return 0;
// }

//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free_mem(uint64 va) {
    va = va - sizeof(pd);
#ifdef MEM_DEBUG
    sprint("=====================\n");
    sprint("to free at sys_user_free_mem: va: %lx\n", va);
    for (pd *i = current->user_heap.mem_rib.alloc_list; i != NULL; i = get_next(i)) {
        sprint("alloc list item: %lx\n", (uint64)i);
    }
#endif
    current->user_heap.mem_rib.free(va, &current->user_heap.mem_rib.free_list, &current->user_heap.mem_rib.alloc_list);
#ifdef MEM_DEBUG
    sprint("after free:\n");
    for (pd *i = current->user_heap.mem_rib.alloc_list; i != NULL; i = get_next(i)) {
        sprint("alloc list item: %lx\n", (uint64)i);
    }
    sprint("===================\n");
#endif

    return 0;
}

//
// reclaim a page, indicated by "va". added @lab2_2
//
inline uint64 sys_user_free_page(uint64 va) {
    sys_user_free_mem(va);
    return 0;
}

//
// kerenl entry point of naive_fork
//
ssize_t sys_user_fork() {
    sprint("User call fork.\n");
    return do_fork(current);
}

//
// kerenl entry point of yield. added @lab3_2
//
ssize_t sys_user_yield() {
    // (lab3_2): implment the syscall of yield.
    // hint: the functionality of yield is to give up the processor. therefore,
    // we should set the status of currently running process to READY, insert it in
    // the rear of ready queue, and finally, schedule a READY process to run.
    current->status = READY;
    insert_to_ready_queue(current);
    schedule();
    return 0;
}

/**
 * @brief 创建一个新的信号量
 * @param initval 信号量的初始值
 * @return 信号量的id，失败返回-1
 */
ssize_t sys_user_sem_new(int initval) {
    semaphore *sem = alloc_semaphore();
    if (sem == NULL) {
        return -1;
    }
    sem->value = initval;
    return sem->sem_id;
}

/**
 * @brief P操作信号量
 * @param semid 信号量的id
 * @return 成功返回0，失败返回-1
 */
ssize_t sys_user_sem_P(int semid) {
    semaphore *sem = get_semaphore(semid);
    if (sem == NULL) {
        return -1;
    } else {
        semaphore_P(sem);
        return 0;
    }
}

/**
 * @brief V操作信号量
 * @param semid 信号量的id
 * @return 成功返回0，失败返回-1
 */
ssize_t sys_user_sem_V(int semid) {
    semaphore *sem = get_semaphore(semid);
    if (sem == NULL) {
        return -1;
    } else {
        semaphore_V(sem);
        return 0;
    }
}

ssize_t sys_user_printpa(uint64 va) {
    uint64 pa = (uint64)user_va_to_pa((pagetable_t)(current->pagetable), (void *)va);
    sprint("%lx\n", pa);
    return 0;
}

//
// open file
//
ssize_t sys_user_open(char *pathva, int flags) {
    char *pathpa = (char *)user_va_to_pa((pagetable_t)(current->pagetable), pathva);
    return do_open(pathpa, flags);
}

//
// read file
//
ssize_t sys_user_read(int fd, char *bufva, uint64 count) {
    int i = 0;
    while (i < count) { // count can be greater than page size
        uint64 addr = (uint64)bufva + i;
        uint64 pa = lookup_pa((pagetable_t)current->pagetable, addr);
        uint64 off = addr - ROUNDDOWN(addr, PGSIZE);
        uint64 len = count - i < PGSIZE - off ? count - i : PGSIZE - off;
        uint64 r = do_read(fd, (char *)pa + off, len);
        i += r;
        if (r < len) return i;
    }
    return count;
}

//
// write file
//
ssize_t sys_user_write(int fd, char *bufva, uint64 count) {
    int i = 0;
    while (i < count) { // count can be greater than page size
        uint64 addr = (uint64)bufva + i;
        uint64 pa = lookup_pa((pagetable_t)current->pagetable, addr);
        uint64 off = addr - ROUNDDOWN(addr, PGSIZE);
        uint64 len = count - i < PGSIZE - off ? count - i : PGSIZE - off;
        uint64 r = do_write(fd, (char *)pa + off, len);
        i += r;
        if (r < len) return i;
    }
    return count;
}

//
// lseek file
//
ssize_t sys_user_lseek(int fd, int offset, int whence) {
    return do_lseek(fd, offset, whence);
}

//
// read vinode
//
ssize_t sys_user_stat(int fd, struct istat *istat) {
    struct istat *pistat = (struct istat *)user_va_to_pa((pagetable_t)(current->pagetable), istat);
    return do_stat(fd, pistat);
}

//
// read disk inode
//
ssize_t sys_user_disk_stat(int fd, struct istat *istat) {
    struct istat *pistat = (struct istat *)user_va_to_pa((pagetable_t)(current->pagetable), istat);
    return do_disk_stat(fd, pistat);
}

//
// close file
//
ssize_t sys_user_close(int fd) {
    return do_close(fd);
}

//
// lib call to opendir
//
ssize_t sys_user_opendir(char *pathva) {
    char *pathpa = (char *)user_va_to_pa((pagetable_t)(current->pagetable), pathva);
    return do_opendir(pathpa);
}

//
// lib call to readdir
//
ssize_t sys_user_readdir(int fd, struct dir *vdir) {
    struct dir *pdir = (struct dir *)user_va_to_pa((pagetable_t)(current->pagetable), vdir);
    return do_readdir(fd, pdir);
}

//
// lib call to mkdir
//
ssize_t sys_user_mkdir(char *pathva) {
    char *pathpa = (char *)user_va_to_pa((pagetable_t)(current->pagetable), pathva);
    return do_mkdir(pathpa);
}

//
// lib call to closedir
//
ssize_t sys_user_closedir(int fd) {
    return do_closedir(fd);
}

//
// lib call to link
//
ssize_t sys_user_link(char *vfn1, char *vfn2) {
    char *pfn1 = (char *)user_va_to_pa((pagetable_t)(current->pagetable), (void *)vfn1);
    char *pfn2 = (char *)user_va_to_pa((pagetable_t)(current->pagetable), (void *)vfn2);
    return do_link(pfn1, pfn2);
}

//
// lib call to unlink
//
ssize_t sys_user_unlink(char *vfn) {
    char *pfn = (char *)user_va_to_pa((pagetable_t)(current->pagetable), (void *)vfn);
    return do_unlink(pfn);
}

/**
 * @brief kernel entry point of wait
 *
 * @version 0.1
 * @author EpsilonZYJ (yujie.zhou05@outlook.com)
 * @date 2025-11-18
 * @copyright Copyright (c) 2025
 */
ssize_t sys_user_wait(int64 pid) {
    return do_wait(pid);
}

ssize_t sys_user_exec(char *command, char *para) {
    char *pfn1 = (char *)user_va_to_pa((pagetable_t)current->pagetable, (void *)command);
    char *pfn2 = (char *)user_va_to_pa((pagetable_t)current->pagetable, (void *)para);
    return do_exec(pfn1, pfn2);
}

//
// lib call to read current working directory
//
ssize_t sys_user_rcwd(char *pathva) {
    char *pathpa = (char *)user_va_to_pa((pagetable_t)(current->pagetable), pathva);
    return do_rcwd(pathpa);
}

//
// lib call to change current working directory
//
ssize_t sys_user_ccwd(char *pathva) {
    char *pathpa = (char *)user_va_to_pa((pagetable_t)(current->pagetable), pathva);
    return do_ccwd(pathpa);
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
        return sys_user_allocate_page();
    case SYS_user_free_page:
        return sys_user_free_page(a1);
    case SYS_user_allocate_mem:
        return sys_user_allocate_mem(a1);
    case SYS_user_free_mem:
        return sys_user_free_mem(a1);
    case SYS_user_fork:
        return sys_user_fork();
    case SYS_user_yield:
        return sys_user_yield();
    // added @lab4_1
    case SYS_user_open:
        return sys_user_open((char *)a1, a2);
    case SYS_user_read:
        return sys_user_read(a1, (char *)a2, a3);
    case SYS_user_write:
        return sys_user_write(a1, (char *)a2, a3);
    case SYS_user_lseek:
        return sys_user_lseek(a1, a2, a3);
    case SYS_user_stat:
        return sys_user_stat(a1, (struct istat *)a2);
    case SYS_user_disk_stat:
        return sys_user_disk_stat(a1, (struct istat *)a2);
    case SYS_user_close:
        return sys_user_close(a1);
    // added @lab4_2
    case SYS_user_opendir:
        return sys_user_opendir((char *)a1);
    case SYS_user_readdir:
        return sys_user_readdir(a1, (struct dir *)a2);
    case SYS_user_mkdir:
        return sys_user_mkdir((char *)a1);
    case SYS_user_closedir:
        return sys_user_closedir(a1);
    // added @lab4_3
    case SYS_user_link:
        return sys_user_link((char *)a1, (char *)a2);
    case SYS_user_unlink:
        return sys_user_unlink((char *)a1);
    case SYS_user_wait:
        return sys_user_wait(a1);
    case SYS_user_exec:
        return sys_user_exec((char *)a1, (char *)a2);
    // added @lab4_challenge1
    case SYS_user_rcwd:
        return sys_user_rcwd((char *)a1);
    case SYS_user_ccwd:
        return sys_user_ccwd((char *)a1);
    case SYS_user_printpa:
        return sys_user_printpa(a1);
    case SYS_user_sem_new:
        return sys_user_sem_new(a1);
    case SYS_user_sem_P:
        return sys_user_sem_P(a1);
    case SYS_user_sem_V:
        return sys_user_sem_V(a1);
    default:
        panic("Unknown syscall %ld \n", a0);
    }
}
