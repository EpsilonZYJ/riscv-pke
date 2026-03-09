#ifndef _PROC_H_
#define _PROC_H_

#include "riscv.h"
#include "proc_file.h"

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

// riscv-pke kernel supports at most 32 processes
#define NPROC 32
// maximum number of pages in a process's heap
#define MAX_HEAP_PAGES 32

// possible status of a process
enum proc_status {
    FREE,    // unused state
    READY,   // ready state
    RUNNING, // currently running
    BLOCKED, // waiting for something
    ZOMBIE,  // terminated but not reclaimed yet
};

// types of a segment
enum segment_type {
    STACK_SEGMENT = 0, // runtime stack segment
    CONTEXT_SEGMENT,   // trapframe segment
    SYSTEM_SEGMENT,    // system segment
    HEAP_SEGMENT,      // runtime heap segment
    CODE_SEGMENT,      // ELF segment
    DATA_SEGMENT,      // ELF segment
};

// the VM regions mapped to a user process
typedef struct mapped_region {
    uint64 va;       // mapped virtual address
    uint32 npages;   // mapping_info is unused if npages == 0
    uint32 seg_type; // segment type, one of the segment_types
} mapped_region;

typedef struct process_heap_manager {
    uint64 heap_top;
    uint64 heap_bottom;
    m_rib mem_rib; // 内存管理信息
} process_heap_manager;

// code file struct, including directory index and file name char pointer
typedef struct {
    uint64 dir;
    char *file;
} code_file;

// address-line number-file name table
typedef struct {
    uint64 addr, line, file;
} addr_line;

// the extremely simple definition of process, used for begining labs of PKE
typedef struct process_t {
    // pointing to the stack used in trap handling.
    uint64 kstack;
    // user page table
    pagetable_t pagetable;
    // trapframe storing the context of a (User mode) process.
    trapframe *trapframe;

    process_heap_manager user_heap; // 堆管理器

    // points to a page that contains mapped_regions. below are added @lab3_1
    mapped_region *mapped_info;
    // next free mapped region in mapped_info
    int total_mapped_region;

    // process id
    uint64 pid;
    // process status
    int status;
    // parent process
    struct process_t *parent;
    // next queue element
    struct process_t *queue_next;

    // accounting. added @lab3_3
    int tick_count;

    // file system. added @lab4_1
    proc_file_management *pfiles;

    // added @lab1_challenge2
    char *debugline;
    char **dir;
    code_file *file;
    addr_line *line;
    int line_ind;
} process;

// switch to run user app
void switch_to(process *);

// initialize process pool (the procs[] array)
void init_proc_pool();
// allocate an empty process, init its vm space. returns its pid
process *alloc_process();
void release_proc_slot_reservation(uint64 pid);
// reclaim a process, destruct its vm space and free physical pages.
int free_process(process *proc);

// current running process
extern process *current[NCPU];

extern process *block_queue_head[NCPU];

int pd_first_fit_cmp(pd *a, pd *b);
void sort_pd_list_ascend(pd **plist_head, pd **changed_item, int (*ascend_cmp)(pd *, pd *));
void merge_free_blocks(pd **p_free_list, int (*ascend_cmp)(pd *, pd *));
void insert_alloc_block(pd **p_alloc_list, pd *new_alloc_block);
void insert_free_block(pd **p_free_list, pd *new_free_block, int (*ascend_cmp)(pd *, pd *));
void sort_free_list_ascend(pd **plist_head, int (*ascend_cmp)(pd *, pd *));
void remove_from_pd_list(pd **plist_head, pd *item);
void set_next(pd *node, pd *next);
pd *get_next(pd *node);
void set_flag(pd *node, int flag);
int get_flag(pd *node);
void set_size(pd *node, uint64 size);
uint64 get_size(pd *node);

// 首次适应分配算法
uint64 first_fit_alloc(uint64 size, pd **p_free_list, pd **p_alloc_list);
// 释放内存
uint64 first_fit_free(uint64 addr, pd **p_free_list, pd **p_alloc_list);

#endif
