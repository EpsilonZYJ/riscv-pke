#ifndef _PROC_H_
#define _PROC_H_

#include "riscv.h"

typedef struct pd_t {
    int flag;          // 0空闲，1已用
    uint64 size;       // 大小，单位：字节
    struct pd_t *next; // 勾链字
} pd;

typedef struct m_rib_t {
    pd *alloc_list;                                                    // 已分配链表头指针
    pd *free_list;                                                     // 空闲链表头指针
    uint64 (*alloc)(uint64 size, pd **p_free_list, pd **p_alloc_list); // 分配内存函数指针
    uint64 (*free)(uint64 addr, pd **p_free_list, pd **p_alloc_list);  // 释放内存函数指针
} m_rib;

typedef struct trapframe_t {
    // space to store context (all common registers)
    /* offset:0   */ riscv_regs regs;

    // process's "user kernel" stack
    /* offset:248 */ uint64 kernel_sp;
    // pointer to smode_trap_handler
    /* offset:256 */ uint64 kernel_trap;
    // saved user process counter
    /* offset:264 */ uint64 epc;

    // kernel page table. added @lab2_1
    /* offset:272 */ uint64 kernel_satp;
} trapframe;

// the extremely simple definition of process, used for begining labs of PKE
typedef struct process_t {
    // pointing to the stack used in trap handling.
    uint64 kstack;
    // user page table
    pagetable_t pagetable;
    // trapframe storing the context of a (User mode) process.
    trapframe *trapframe;
    m_rib mem_rib; // 内存管理信息
} process;

// switch to run user app
void switch_to(process *);

// current running process
extern process *current;

// address of the first free page in our simple heap. added @lab2_2
extern uint64 g_ufree_page;

int pd_first_fit_cmp(pd *a, pd *b);
void sort_pd_list_ascend(pd **plist_head, pd **changed_item, int (*ascend_cmp)(pd *, pd *));
void merge_free_blocks(pd **p_free_list, int (*ascend_cmp)(pd *, pd *));
void insert_free_block(pd **p_free_list, pd *new_free_block, int (*ascend_cmp)(pd *, pd *));
void sort_free_list_ascend(pd **plist_head, int (*ascend_cmp)(pd *, pd *));

// 首次适应分配算法
uint64 first_fit_alloc(uint64 size, pd **p_free_list, pd **p_alloc_list);
// 释放内存
uint64 first_fit_free(uint64 addr, pd **p_free_list, pd **p_alloc_list);

#endif
