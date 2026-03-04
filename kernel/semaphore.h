//
// Created by 周煜杰 on 2026/1/29.
//

#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

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

#endif // _SEMAPHORE_H_
