#ifndef _SCHED_H_
#define _SCHED_H_

#include "process.h"

// length of a time slice, in number of ticks
#define TIME_SLICE_LEN 2

void insert_to_ready_queue(process *proc);
void insert_to_block_queue(process **pblock_queue_head, process *proc);
process *wake_from_block_queue(process **pblock_queue_head, process *child_process);
process *remove_from_block_queue(process **pblock_queue_head, process *proc);
process *pick_and_remove_from_block_queue(process **pblock_queue_head);
void schedule();

#include "riscv.h"
#include "util/types.h"
#include "process.h"

// 信号量最大支持数目
#define NSEMAPHORE 16

typedef ssize_t semid_t;

enum semaphore_status {
    SEM_FREE, // 未使用
    SEM_USED, // 已使用
};

typedef struct semaphore_t {
    semid_t sem_id;               // 信号量ID
    ssize_t value;                // 信号量的当前值
    enum semaphore_status status; // 信号量状态
    process *proc_queue_head;     // 等待该信号量的进程队列头
} semaphore;

void init_semaphore_pool();
semaphore *alloc_semaphore();
semaphore *get_semaphore(semid_t semid);
void free_semaphore(semaphore *sem);
void semaphore_P(semaphore *sem);
void semaphore_V(semaphore *sem);

#endif
