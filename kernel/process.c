/*
 * Utility functions for process management.
 *
 * Note: in Lab1, only one process (i.e., our user application) exists. Therefore,
 * PKE OS at this stage will set "current" to the loaded user application, and also
 * switch to the old "current" process after trap handling.
 */

#include "riscv.h"
#include "strap.h"
#include "config.h"
#include "process.h"
#include "elf.h"
#include "string.h"
#include "vmm.h"
#include "pmm.h"
#include "memlayout.h"
#include "sched.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"

// Two functions defined in kernel/usertrap.S
extern char smode_trap_vector[];
extern void return_to_user(trapframe *, uint64 satp);

// trap_sec_start points to the beginning of S-mode trap segment (i.e., the entry point
// of S-mode trap vector).
extern char trap_sec_start[];

// process pool. added @lab3_1
process procs[NPROC];

// current points to the currently running user-mode application.
process *current = NULL;

process *block_queue_head = NULL;

//
// switch to a user-mode process
//
void switch_to(process *proc) {
    assert(proc);
    current = proc;

    // write the smode_trap_vector (64-bit func. address) defined in kernel/strap_vector.S
    // to the stvec privilege register, such that trap handler pointed by smode_trap_vector
    // will be triggered when an interrupt occurs in S mode.
    write_csr(stvec, (uint64)smode_trap_vector);

    // set up trapframe values (in process structure) that smode_trap_vector will need when
    // the process next re-enters the kernel.
    proc->trapframe->kernel_sp = proc->kstack;     // process's kernel stack
    proc->trapframe->kernel_satp = read_csr(satp); // kernel page table
    proc->trapframe->kernel_trap = (uint64)smode_trap_handler;

    // SSTATUS_SPP and SSTATUS_SPIE are defined in kernel/riscv.h
    // set S Previous Privilege mode (the SSTATUS_SPP bit in sstatus register) to User mode.
    unsigned long x = read_csr(sstatus);
    x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE; // enable interrupts in user mode

    // write x back to 'sstatus' register to enable interrupts, and sret destination mode.
    write_csr(sstatus, x);

    // set S Exception Program Counter (sepc register) to the elf entry pc.
    write_csr(sepc, proc->trapframe->epc);

    // make user page table. macro MAKE_SATP is defined in kernel/riscv.h. added @lab2_1
    uint64 user_satp = MAKE_SATP(proc->pagetable);

    // return_to_user() is defined in kernel/strap_vector.S. switch to user mode with sret.
    // note, return_to_user takes two parameters @ and after lab2_1.
    return_to_user(proc->trapframe, user_satp);
}

//
// initialize process pool (the procs[] array). added @lab3_1
//
void init_proc_pool() {
    memset(procs, 0, sizeof(process) * NPROC);

    for (int i = 0; i < NPROC; ++i) {
        procs[i].status = FREE;
        procs[i].pid = i;
    }
}

//
// allocate an empty process, init its vm space. returns the pointer to
// process strcuture. added @lab3_1
//
process *alloc_process() {
    // locate the first usable process structure
    int i;

    for (i = 0; i < NPROC; i++)
        if (procs[i].status == FREE) break;

    if (i >= NPROC) {
        panic("cannot find any free process structure.\n");
        return 0;
    }

    // init proc[i]'s vm space
    procs[i].trapframe = (trapframe *)alloc_page(); // trapframe, used to save context
    memset(procs[i].trapframe, 0, sizeof(trapframe));

    // page directory
    procs[i].pagetable = (pagetable_t)alloc_page();
    memset((void *)procs[i].pagetable, 0, PGSIZE);

    procs[i].kstack = (uint64)alloc_page() + PGSIZE; // user kernel stack top
    uint64 user_stack = (uint64)alloc_page();        // phisical address of user stack bottom
    procs[i].trapframe->regs.sp = USER_STACK_TOP;    // virtual address of user stack top

    // allocates a page to record memory regions (segments)
    procs[i].mapped_info = (mapped_region *)alloc_page();
    memset(procs[i].mapped_info, 0, PGSIZE);

    // map user stack in userspace
    user_vm_map((pagetable_t)procs[i].pagetable, USER_STACK_TOP - PGSIZE, PGSIZE,
                user_stack, prot_to_type(PROT_WRITE | PROT_READ, 1));
    procs[i].mapped_info[STACK_SEGMENT].va = USER_STACK_TOP - PGSIZE;
    procs[i].mapped_info[STACK_SEGMENT].npages = 1;
    procs[i].mapped_info[STACK_SEGMENT].seg_type = STACK_SEGMENT;

    // map trapframe in user space (direct mapping as in kernel space).
    user_vm_map((pagetable_t)procs[i].pagetable, (uint64)procs[i].trapframe, PGSIZE,
                (uint64)procs[i].trapframe, prot_to_type(PROT_WRITE | PROT_READ, 0));
    procs[i].mapped_info[CONTEXT_SEGMENT].va = (uint64)procs[i].trapframe;
    procs[i].mapped_info[CONTEXT_SEGMENT].npages = 1;
    procs[i].mapped_info[CONTEXT_SEGMENT].seg_type = CONTEXT_SEGMENT;

    // map S-mode trap vector section in user space (direct mapping as in kernel space)
    // we assume that the size of usertrap.S is smaller than a page.
    user_vm_map((pagetable_t)procs[i].pagetable, (uint64)trap_sec_start, PGSIZE,
                (uint64)trap_sec_start, prot_to_type(PROT_READ | PROT_EXEC, 0));
    procs[i].mapped_info[SYSTEM_SEGMENT].va = (uint64)trap_sec_start;
    procs[i].mapped_info[SYSTEM_SEGMENT].npages = 1;
    procs[i].mapped_info[SYSTEM_SEGMENT].seg_type = SYSTEM_SEGMENT;

    sprint("in alloc_proc. user frame 0x%lx, user stack 0x%lx, user kstack 0x%lx \n",
           procs[i].trapframe, procs[i].trapframe->regs.sp, procs[i].kstack);

    procs[i].user_heap.mem_rib.alloc_list = NULL;
    procs[i].user_heap.mem_rib.free_list = NULL;
    procs[i].user_heap.mem_rib.alloc = ALLOC_FUNC;
    procs[i].user_heap.mem_rib.free = FREE_FUNC;
    procs[i].user_heap.heap_top = USER_FREE_ADDRESS_START;
    procs[i].user_heap.heap_bottom = USER_FREE_ADDRESS_START;

    // map user heap in userspace
    procs[i]
        .mapped_info[HEAP_SEGMENT]
        .va = USER_FREE_ADDRESS_START;
    procs[i].mapped_info[HEAP_SEGMENT].npages = 0; // no pages are mapped to heap yet.
    procs[i].mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;

    procs[i].total_mapped_region = 4;

    // initialize files_struct
    procs[i].pfiles = init_proc_file_management();
    sprint("in alloc_proc. build proc_file_management successfully.\n");

    // return after initialization.
    return &procs[i];
}

//
// reclaim a process. added @lab3_1
//
int free_process(process *proc) {
    // we set the status to ZOMBIE, but cannot destruct its vm space immediately.
    // since proc can be current process, and its user kernel stack is currently in use!
    // but for proxy kernel, it (memory leaking) may NOT be a really serious issue,
    // as it is different from regular OS, which needs to run 7x24.
    proc->status = ZOMBIE;

    return 0;
}

void insert_alloc_block(pd **p_alloc_list, pd *new_alloc_block) {
    if (*p_alloc_list == NULL && new_alloc_block == NULL) return;
    if (*p_alloc_list == NULL && new_alloc_block != NULL) {
        *p_alloc_list = new_alloc_block;
        new_alloc_block->next = NULL;
        return;
    }
    new_alloc_block->next = *p_alloc_list;
    *p_alloc_list = new_alloc_block;
    return;
}

// 插入空闲块到空闲链表
void insert_free_block(pd **p_free_list, pd *new_free_block, int (*ascend_cmp)(pd *, pd *)) {
    if (ascend_cmp == NULL) return;
    if (*p_free_list == NULL && new_free_block == NULL) return;
    if (*p_free_list == NULL && new_free_block != NULL) {
        *p_free_list = new_free_block;
        new_free_block->next = NULL;
        return;
    }
    new_free_block->next = *p_free_list;
    *p_free_list = new_free_block;
    sort_pd_list_ascend(p_free_list, &new_free_block, ascend_cmp);
    return;
}

int pd_first_fit_cmp(pd *a, pd *b) {
    return (int)((uint64)a - (uint64)b);
}

// 重新排序内存块链表
void sort_pd_list_ascend(pd **plist_head, pd **changed_item, int (*ascend_cmp)(pd *, pd *)) {
    if (*plist_head == NULL || *changed_item == NULL || ascend_cmp == NULL) {
        return;
    }
    pd *item = *changed_item;
    pd *prev = *plist_head;
    if (prev == item) {
        *plist_head = item->next;
        prev = *plist_head;
    } else {
        while (prev && prev->next != item) {
            prev = prev->next;
        }
        if (prev && prev->next == item) {
            prev->next = item->next;
        } else {
            return;
        }
    }

    if ((*changed_item)->size == 0)
        return;
    else {
        if (*plist_head == NULL || ascend_cmp(item, *plist_head) <= 0) {
            item->next = *plist_head;
            *plist_head = item;
        } else {
            pd *cur = *plist_head;
            while (cur->next && ascend_cmp(cur, item) < 0) {
                prev = cur;
                cur = cur->next;
            }
            if (cur->next == NULL && ascend_cmp(cur, item) < 0) {
                cur->next = item;
                item->next = NULL;
            } else {
                prev->next = item;
                item->next = cur;
            }
        }
    }
}

// 整体排序链表
void sort_free_list_ascend(pd **plist_head, int (*ascend_cmp)(pd *, pd *)) {
    if (*plist_head == NULL || ascend_cmp == NULL) {
        return;
    }
    pd *sorted_list = NULL;
    pd *cur = *plist_head;
    while (cur) {
        pd *next = cur->next;
        insert_free_block(&sorted_list, cur, ascend_cmp);
        cur = next;
    }
    *plist_head = sorted_list;
}

inline uint64 get_page_hash(pd *p) {
    uint64 addr = (uint64)p;
    return (addr - USER_FREE_ADDRESS_START) / PGSIZE;
}

// 合并相邻空闲块
void merge_free_blocks(pd **p_free_list, int (*ascend_cmp)(pd *, pd *)) {
    if (*p_free_list == NULL) {
        return;
    }

    // 先将空闲链表按地址排序，便于合并
    sort_free_list_ascend(p_free_list, PD_CMP_FUNC);

    pd *cur = *p_free_list;
    while (cur && cur->next) {
        pd *next = cur->next;
        if (((uint64)cur + sizeof(pd) + cur->size == (uint64)next)
            && (get_page_hash(cur) == get_page_hash(next))) { // 地址相邻且在同一页
            // 合并
            cur->size = cur->size + sizeof(pd) + next->size;
            cur->next = next->next;
        } else {
            cur = cur->next;
        }
    }
    // 重新按照要求排序空闲链表
    sort_free_list_ascend(p_free_list, ascend_cmp);
}

// 首次适应分配算法
uint64 first_fit_alloc(uint64 size, pd **p_free_list, pd **p_alloc_list) {
    if (*p_free_list == NULL || size == 0) {
        return (uint64)NULL;
    }
    pd *alloc_item = *p_free_list;
    pd *prev = NULL;

    while (alloc_item) {
        if (alloc_item->size == size) {
            // 刚好合适
            alloc_item->flag = 1;
            alloc_item->size = 0;

            // 从空闲链表中移除
            if (prev) {
                prev->next = alloc_item->next;
            } else {
                *p_free_list = alloc_item->next;
            }

            // 加入已分配链表
            alloc_item->next = *p_alloc_list;
            *p_alloc_list = alloc_item;
            // 在分配块头部记录大小信息，便于释放时使用
            alloc_item->size = size;

            return (uint64)alloc_item;
        } else if (alloc_item->size < size || alloc_item->size <= size + sizeof(pd)) {
            // 空闲块太小或者空闲块不适合拆分，继续找下一个
            prev = alloc_item;
            alloc_item = alloc_item->next;
        } else {
            // 找到合适的空闲块，进行分割
            // 从块的头部（起始位置）进行分配

            uint64 total_free_size = alloc_item->size;
            pd *next_free_node = alloc_item->next;

            // 分配的块就是 alloc_item 本身
            pd *alloc_pd = alloc_item;
            // 新的空闲块在已分配部分之后开始
            uint64 new_free_addr = (uint64)alloc_item + sizeof(pd) + size;
            pd *new_free_item = (pd *)new_free_addr;

            // 设置新的空闲块
            new_free_item->flag = 0;
            new_free_item->size = total_free_size - size - sizeof(pd);
            new_free_item->next = next_free_node;

            // 更新空闲链表
            if (prev) {
                prev->next = new_free_item;
            } else {
                *p_free_list = new_free_item;
            }

            // 设置已分配块
            alloc_pd->flag = 1;
            alloc_pd->size = size;

            // 添加到已分配链表
            alloc_pd->next = *p_alloc_list;
            *p_alloc_list = alloc_pd;

            return (uint64)alloc_pd;
        }
    }
    return (uint64)NULL;
}

// 释放内存
uint64 first_fit_free(uint64 addr, pd **p_free_list, pd **p_alloc_list) {
    if (*p_alloc_list == NULL) {
        panic("free error: no allocated block.\n");
    }
    pd *to_free = *p_alloc_list;
    uint64 size = to_free->size;
    if (addr == (uint64)to_free) {
        to_free->flag = 0;
        // 从已分配链表中移除
        *p_alloc_list = to_free->next;

        // 单页内块：加入空闲链表
        to_free->next = *p_free_list;
        *p_free_list = to_free;
        // 重新排序空闲链表
        sort_pd_list_ascend(p_free_list, &to_free, PD_CMP_FUNC);
        // 合并相邻空闲块
        merge_free_blocks(p_free_list, PD_CMP_FUNC);
        return size;
    }
    pd *prev = to_free;
    to_free = to_free->next;
    if (to_free) {
        size = to_free->size;
    }
    while (to_free) {
        if (addr == (uint64)to_free) {
            to_free->flag = 0;
            // 从已分配链表中移除
            prev->next = to_free->next;

            // 单页内块：加入空闲链表
            to_free->next = *p_free_list;
            *p_free_list = to_free;
            // 重新排序空闲链表
            sort_pd_list_ascend(p_free_list, &to_free, PD_CMP_FUNC);
            // 合并相邻空闲块
            merge_free_blocks(p_free_list, PD_CMP_FUNC);
            return size;
        } else {
            prev = to_free;
            to_free = to_free->next;
            if (to_free) {
                size = to_free->size;
            }
        }
    }
    panic("free error: cannot find the allocated block.\n");
}

void remove_from_pd_list(pd **plist_head, pd *item) {
    if (*plist_head == NULL || item == NULL) {
        return;
    }
    pd *prev = *plist_head;
    if (prev == item) {
        *plist_head = item->next;
        return;
    }
    while (prev && prev->next != item) {
        prev = prev->next;
    }
    if (prev && prev->next == item) {
        prev->next = item->next;
    }
}