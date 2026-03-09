//
// Created by 周煜杰 on 2026/1/29.
//

#include "util/types.h"
#include "semaphore.h"
#include "lock.h"
#include "process.h"
#include "sched.h"
#include "spike_interface/spike_utils.h"
#include "config.h"

semaphore semaphores[NSEMAPHORE];

static volatile semid_t g_next_semaphore_id = 0;
static volatile int g_semaphore_lock = 0;
static volatile int g_semaphore_pool_lock = 0;

static inline semid_t alloc_semaphore_id() {
    semid_t semid;
    spin_lock(&g_semaphore_lock);
    semid = g_next_semaphore_id;
    g_next_semaphore_id++;
    spin_unlock(&g_semaphore_lock);
    return semid;
}

void init_semaphore_pool() {
    spin_lock(&g_semaphore_pool_lock);
    for (int i = 0; i < NSEMAPHORE; i++) {
        semaphores[i].sem_id = -1;
        semaphores[i].value = -1;
        semaphores[i].status = SEM_FREE;
    }
    spin_unlock(&g_semaphore_pool_lock);
}

semaphore *alloc_semaphore() {
    semid_t id = alloc_semaphore_id();
    spin_lock(&g_semaphore_pool_lock);
    for (int i = 0; i < NSEMAPHORE; i++) {
        if (semaphores[i].status == SEM_FREE) {
            semaphores[i].sem_id = id;
            semaphores[i].status = SEM_USED;
            spin_unlock(&g_semaphore_pool_lock);
            return &semaphores[i];
        }
    }
    spin_unlock(&g_semaphore_pool_lock);
    return NULL;
}

semaphore *get_semaphore(semid_t semid) {
    for (int i = 0; i < NSEMAPHORE; i++) {
        if (semaphores[i].sem_id == semid && semaphores[i].status == SEM_USED) {
            return &semaphores[i];
        }
    }
    return NULL;
}

void free_semaphore(semaphore *sem) {
    spin_lock(&g_semaphore_pool_lock);
    sem->sem_id = -1;
    sem->value = -1;
    sem->status = SEM_FREE;
    spin_unlock(&g_semaphore_pool_lock);
}

void semaphore_P(semaphore *sem) {
    spin_lock(&g_semaphore_lock);
    sem->value--;
    if (sem->value < 0) {
        current->status = BLOCKED;
        insert_to_block_queue(&sem->proc_queue_head, current);
        spin_unlock(&g_semaphore_lock);
        schedule();
    } else {
        spin_unlock(&g_semaphore_lock);
    }
}

void semaphore_V(semaphore *sem) {
    spin_lock(&g_semaphore_lock);
    sem->value++;
    if (sem->value <= 0) {
        process *proc = pick_and_remove_from_block_queue(&sem->proc_queue_head);
        if (proc == NULL) {
            panic("semaphore_V: no process to wake up.\n");
        } else {
            proc->status = READY;
            insert_to_ready_queue(proc);
            spin_unlock(&g_semaphore_lock);
            return;
        }
    } else {
        spin_unlock(&g_semaphore_lock);
        return;
    }
}
