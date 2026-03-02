/*
 * implementing the scheduler
 */

#include "sched.h"
#include "spike_interface/spike_utils.h"
#include "lock.h"
#include "process.h"
#include "sched.h"
#include "spike_interface/spike_utils.h"
#include "util/types.h"

process *ready_queue_head = NULL;
// process *block_queue_head = NULL;

//
// insert a process, proc, into the END of ready queue.
//
void insert_to_ready_queue(process *proc) {
    sprint("going to insert process %d to ready queue.\n", proc->pid);
    // if the queue is empty in the beginning
    if (ready_queue_head == NULL) {
        proc->status = READY;
        proc->queue_next = NULL;
        ready_queue_head = proc;
        return;
    }

    // ready queue is not empty
    process *p;
    // browse the ready queue to see if proc is already in-queue
    for (p = ready_queue_head; p->queue_next != NULL; p = p->queue_next)
        if (p == proc) return; // already in queue

    // p points to the last element of the ready queue
    if (p == proc) return;
    p->queue_next = proc;
    proc->status = READY;
    proc->queue_next = NULL;

    return;
}

void insert_to_block_queue(process **pblock_queue_head, process *proc) {
    if (*pblock_queue_head == NULL) {
        proc->status = BLOCKED;
        proc->queue_next = NULL;
        *pblock_queue_head = proc;
        return;
    }

    process *p;
    // browse the block queue to see if proc is already in-queue
    for (p = *pblock_queue_head; p->queue_next != NULL; p = p->queue_next)
        if (p == proc) return; // already in queue

    // p points to the last element of the block queue
    if (p == proc) return;
    p->queue_next = proc;
    proc->status = BLOCKED;
    proc->queue_next = NULL;

    return;
}

process *remove_from_block_queue(process **pblock_queue_head, process *proc) {
    process *prev = NULL;
    process *p = *pblock_queue_head;

    while (p != NULL) {
        if (p == proc) {
            // found
            if (prev == NULL) {
                // head element
                *pblock_queue_head = p->queue_next;
            } else {
                prev->queue_next = p->queue_next;
            }
            p->queue_next = NULL;
            return p;
        }
        prev = p;
        p = p->queue_next;
    }

    return NULL; // not found
}

process *wake_from_block_queue(process **pblock_queue_head, process *child_process) {
    process *prev = NULL;
    process *p = *pblock_queue_head;

    while (p != NULL) {
        if (child_process == NULL || p->pid == child_process->parent->pid) {
            // found
            if (prev == NULL) {
                // head element
                *pblock_queue_head = p->queue_next;
            } else {
                prev->queue_next = p->queue_next;
            }
            p->queue_next = NULL;
            return p;
        }
        prev = p;
        p = p->queue_next;
    }

    return NULL; // not found
}

process *pick_and_remove_from_block_queue(process **pblock_queue_head) {
    process *proc = *pblock_queue_head;
    return remove_from_block_queue(pblock_queue_head, proc);
}

//
// choose a proc from the ready queue, and put it to run.
// note: schedule() does not take care of previous current process. If the current
// process is still runnable, you should place it into the ready queue (by calling
// ready_queue_insert), and then call schedule().
//
extern process procs[NPROC];
void schedule() {
    if (!ready_queue_head) {
        // by default, if there are no ready process, and all processes are in the status of
        // FREE and ZOMBIE, we should shutdown the emulated RISC-V machine.
        int should_shutdown = 1;

        for (int i = 0; i < NPROC; i++)
            if ((procs[i].status != FREE) && (procs[i].status != ZOMBIE)) {
                should_shutdown = 0;
                sprint("ready queue empty, but process %d is not in free/zombie state:%d\n",
                       i, procs[i].status);
            }

        if (should_shutdown) {
            sprint("no more ready processes, system shutdown now.\n");
            shutdown(0);
        } else {
            panic("Not handled: we should let system wait for unfinished processes.\n");
        }
    }

    current = ready_queue_head;
    assert(current->status == READY);
    ready_queue_head = ready_queue_head->queue_next;

    current->status = RUNNING;
    sprint("going to schedule process %d to run.\n", current->pid);
    switch_to(current);
}

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
    for (int i = 0; i < NSEMAPHORE; i++) {
        semaphores[i].sem_id = -1;
        semaphores[i].value = -1;
        semaphores[i].status = SEM_FREE;
    }
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