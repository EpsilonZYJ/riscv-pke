/*
 * implementing the scheduler
 */

#include "sched.h"
#include "spike_interface/spike_utils.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "sync_utils.h"
#include "lock.h"
#include "debug_config.h"

process *ready_queue_head[NCPU];
static volatile int g_ready_queue_lock[NCPU] = {0}; // per-hart ready queue lock, protects the ready queue of each hart

static volatile int exit_counter = 0;
// 全局关机信号，hart 0 决定关机时置1，其余 hart 通过自旋检测后参与 barrier
static volatile int g_should_shutdown = 0;

//
// insert a process, proc, into the END of ready queue.
//
void insert_to_ready_queue(process *proc) {
    uint64 hartid = read_tp();
    assert(hartid < NCPU);
    insert_to_specific_ready_queue(proc, hartid);
}

//
// insert a process into a SPECIFIC hart's ready queue (used for cross-hart dispatch).
//
void insert_to_specific_ready_queue(process *proc, int target_hartid) {
    assert(target_hartid >= 0 && target_hartid < NCPU);
#ifdef SYSTEM_INFO_OUTPUT
    sprint("going to insert process %d to ready queue.\n", proc->pid);
#endif
    release_proc_slot_reservation(proc->pid); // safe to call multiple times
    // if the queue is empty in the beginning

    spin_lock(&g_ready_queue_lock[target_hartid]);

    if (ready_queue_head[target_hartid] == NULL) {
        if (proc->status != ZOMBIE)
            proc->status = READY;
        proc->queue_next = NULL;
        ready_queue_head[target_hartid] = proc;
        spin_unlock(&g_ready_queue_lock[target_hartid]);
        return;
    }

    // ready queue is not empty
    process *p;
    // browse the ready queue to see if proc is already in-queue
    for (p = ready_queue_head[target_hartid]; p->queue_next != NULL; p = p->queue_next) {
        if (p == proc) {
            spin_unlock(&g_ready_queue_lock[target_hartid]);
            return;
        } // already in queue
    }

    // p points to the last element of the ready queue
    if (p == proc) {
        spin_unlock(&g_ready_queue_lock[target_hartid]);
        return;
    }
    p->queue_next = proc;
    if (proc->status != ZOMBIE)
        proc->status = READY;
    proc->queue_next = NULL;
    spin_unlock(&g_ready_queue_lock[target_hartid]);
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
    uint64 hartid = read_tp();
    assert(hartid < NCPU);

    spin_lock(&g_ready_queue_lock[hartid]);
    int condition = !ready_queue_head[hartid];
    spin_unlock(&g_ready_queue_lock[hartid]);

    if (condition) {
        // 本 hart 就绪队列为空时：
        // 1. 若全部进程已完成（FREE/ZOMBIE），则这是真正的关机时机。
        // 2. 若存在仍在运行/阻塞的进程（可能在其他 hart 上），则自旋等待新任务或关机信号。
        int global_all_done = 1;
        for (int i = 0; i < NPROC; i++) {
#ifdef SYSTEM_INFO_OUTPUT
            switch (procs[i].status) {
            case FREE:
                sprint("hartid = %d FREE: process %d.\n", hartid, procs[i].pid);
                break;
            case READY:
                sprint("hartid = %d READY: process %d.\n", hartid, procs[i].pid);
                break;
            case RUNNING:
                sprint("hartid = %d RUNNING: process %d.\n", hartid, procs[i].pid);
                break;
            case BLOCKED:
                sprint("hartid = %d BLOCKED: process %d.\n", hartid, procs[i].pid);
                break;
            case ZOMBIE:
                sprint("hartid = %d ZOMBIE: process %d.\n", hartid, procs[i].pid);
                break;
            default:
                sprint("hartid = %d UNKNOWN STATUS: process %d.\n", hartid, procs[i].pid);
                break;
            }
#endif

            if ((procs[i].status != FREE) && (procs[i].status != ZOMBIE)) {
                global_all_done = 0;
                break;
            }
        }

        if (global_all_done) {
            // 所有进程均已结束，触发关机流程
            g_should_shutdown = 1;
#ifdef SYSTEM_INFO_OUTPUT
            sprint("^^^^^^^^hartid = %d: no more ready processes, going to shutdown now^^^^^^^^^^\n", hartid);
            sprint("hartid = %d: no more ready processes, system shutdown now.\n", hartid);
#endif
            if (hartid == 0) {
                sync_barrier(&exit_counter, NCPU);
                shutdown(0);
            } else {
                sync_barrier(&exit_counter, NCPU);
                while (1); // hart 0 调用 shutdown() 终止仿真，此处实际不可达
            }
        } else {
// 全局仍有进程，本 hart 自旋等待新任务派发或全局关机信号
#ifdef SYSTEM_INFO_OUTPUT
            sprint("^^^^^^^^^^still have processes running, hartid = %d is going to sleep now^^^^^^^^^^\n", hartid);
#endif

            while (!g_should_shutdown) {
                // 自旋等待
                spin_lock(&g_ready_queue_lock[hartid]);
                int ready_to_shut_down = !ready_queue_head[hartid];
                spin_unlock(&g_ready_queue_lock[hartid]);
                if (!ready_to_shut_down) {
                    // 就绪队列已有新任务，继续往下调度
                    break;
                }
            }
            if (g_should_shutdown) {
                // 其他 hart 已决定关机，参与 barrier 后等待终止
                sync_barrier(&exit_counter, NCPU);
                while (1); // 不可达
            }
            // 就绪队列已有新任务，继续往下调度
        }
    }

    current[hartid] = ready_queue_head[hartid];
    assert(current[hartid]->status == READY);
    ready_queue_head[hartid] = ready_queue_head[hartid]->queue_next;

    current[hartid]->status = RUNNING;
#ifdef SYSTEM_INFO_OUTPUT
    sprint("hartid = %d: going to schedule process %d to run.\n", hartid, current[hartid]->pid);
#endif
    switch_to(current[hartid]);
}

//
// implements fork syscal in kernel. added @lab3_1
// basic idea here is to first allocate an empty process (child), then duplicate the
// context and data segments of parent process to the child, and lastly, map other
// segments (code, system) of the parent to child. the stack segment remains unchanged
// for the child.
//
int do_fork(process *parent) {
#ifdef DO_FORK_OUTPUT
    sprint("will fork a child from parent %d.\n", parent->pid);
#endif
    process *child = alloc_process();

    for (int i = 0; i < parent->total_mapped_region; i++) {
        // browse parent's vm space, and copy its trapframe and data segments,
        // map its code segment.
        switch (parent->mapped_info[i].seg_type) {
        case CONTEXT_SEGMENT:
            *child->trapframe = *parent->trapframe;
            break;
        case STACK_SEGMENT:
            memcpy((void *)lookup_pa(child->pagetable, child->mapped_info[STACK_SEGMENT].va),
                   (void *)lookup_pa(parent->pagetable, parent->mapped_info[i].va), PGSIZE);
            break;
        case HEAP_SEGMENT: { // build a same heap for child process.
            // copy and map the heap blocks
            for (uint64 heap_block = parent->user_heap.heap_bottom;
                 heap_block < parent->user_heap.heap_top; heap_block += PGSIZE) {
                pte_t *pte = page_walk(parent->pagetable, heap_block, 0);
                if (pte && (*pte & PTE_V)) {
                    uint64 pa = PTE2PA(*pte);
                    inc_page_ref((void *)pa);
                    uint64 flags = PTE_FLAGS(*pte);
                    flags &= ~PTE_W;  // clear the writable bit
                    flags |= PTE_COW; // set the copy-on-write bit
                    user_vm_map(child->pagetable, heap_block, PGSIZE, pa, flags | PTE_V);
                    *pte = PA2PTE(pa) | flags | PTE_V; // update the parent's pte to be copy-on-write as well
                    flush_tlb();
                }
            }

            child->mapped_info[HEAP_SEGMENT].npages = parent->mapped_info[HEAP_SEGMENT].npages;
            child->mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;
            child->mapped_info[HEAP_SEGMENT].va = parent->mapped_info[HEAP_SEGMENT].va;

            // copy the heap manager from parent to child.
            // NOTE: keep copied lists, COW page fault handler will remap list pointers
            // from old physical page to new copied page when needed.
            memcpy((void *)&child->user_heap, (void *)&parent->user_heap, sizeof(parent->user_heap));
            break;
        }
        case CODE_SEGMENT:
            // (lab3_1): implment the mapping of child code segment to parent's
            // code segment.
            // hint: the virtual address mapping of code segment is tracked in mapped_info
            // page of parent's process structure. use the information in mapped_info to
            // retrieve the virtual to physical mapping of code segment.
            // after having the mapping information, just map the corresponding virtual
            // address region of child to the physical pages that actually store the code
            // segment of parent process.
            // DO NOT COPY THE PHYSICAL PAGES, JUST MAP THEM.

            for (int j = 0; j < parent->mapped_info[i].npages; j++) {
                uint64 addr = lookup_pa(parent->pagetable, parent->mapped_info[i].va + j * PGSIZE);
                map_pages(child->pagetable, parent->mapped_info[i].va + j * PGSIZE, PGSIZE, addr,
                          prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));
#ifdef DO_FORK_OUTPUT
                sprint("do_fork map code segment at pa:%lx of parent to child at va:%lx.\n", addr, parent->mapped_info[i].va + j * PGSIZE);
#endif
            }

            // after mapping, register the vm region (do not delete codes below!)
            child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
            child->mapped_info[child->total_mapped_region].npages =
                parent->mapped_info[i].npages;
            child->mapped_info[child->total_mapped_region].seg_type = CODE_SEGMENT;
            child->total_mapped_region++;
            break;
        case DATA_SEGMENT:
            for (int j = 0; j < parent->mapped_info[i].npages; j++) {
                void *child_pa = alloc_page();
                uint64 parent_va = parent->mapped_info[i].va + j * PGSIZE;
                void *parent_pa = (void *)lookup_pa(parent->pagetable, parent_va);
                memcpy(child_pa, parent_pa, PGSIZE);
                user_vm_map((pagetable_t)child->pagetable, parent_va, PGSIZE, (uint64)child_pa,
                            prot_to_type(PROT_WRITE | PROT_READ, 1));
            }

            child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
            child->mapped_info[child->total_mapped_region].npages = parent->mapped_info[i].npages;
            child->mapped_info[child->total_mapped_region].seg_type = DATA_SEGMENT;
            child->total_mapped_region++;
            break;
        }
    }

    child->status = READY;
    child->trapframe->regs.a0 = 0;
    child->parent = parent;
    insert_to_ready_queue(child);

    return child->pid;
}

int do_wait(int64 pid) {
    uint64 hartid = read_tp();
    assert(hartid < NCPU);
    int has_child = 0;
    if (pid > 0) {
        if (pid >= NPROC || procs[pid].parent != current[hartid]) {
            // pid大于0但不是当前子进程或不合法
            return -1;
        } else {
            has_child = 1;
        }
    } else if (pid == -1) {
        // pid为-1时，等待任意子进程结束
        for (int i = 0; i < NPROC; i++) {
            // 允许父进程等待运行在任意 hart 上的子进程（多核场景）
            if (procs[i].parent == current[hartid] && procs[i].status != FREE) {
                has_child = 1;
                if (procs[i].status == ZOMBIE) {
                    // 已结束
                    procs[i].status = FREE;
                    return procs[i].pid;
                }
                break;
            }
        }
    }

    if (!has_child) {
        return -1; // 没有子进程
    }

    current[hartid]->status = BLOCKED;
    insert_to_block_queue(&block_queue_head[hartid], current[hartid]);

    schedule();
    return -1; // 不会执行到这里
}

//
// 在指定 hart 上 fork 一个子进程。与 do_fork 逻辑相同，
// 但将子进程插入 target_hartid 的就绪队列，并修正 tp 寄存器。
//
int do_fork_to_hart(process *parent, int target_hartid) {
    assert(target_hartid >= 0 && target_hartid < NCPU);
#ifdef DO_FORK_OUTPUT
    sprint("will fork a child from parent %d to hart %d.\n", parent->pid, target_hartid);
#endif
    process *child = alloc_process();

    for (int i = 0; i < parent->total_mapped_region; i++) {
        switch (parent->mapped_info[i].seg_type) {
        case CONTEXT_SEGMENT:
            *child->trapframe = *parent->trapframe;
            break;
        case STACK_SEGMENT:
            memcpy((void *)lookup_pa(child->pagetable, child->mapped_info[STACK_SEGMENT].va),
                   (void *)lookup_pa(parent->pagetable, parent->mapped_info[i].va), PGSIZE);
            break;
        case HEAP_SEGMENT: {
            for (uint64 heap_block = parent->user_heap.heap_bottom;
                 heap_block < parent->user_heap.heap_top; heap_block += PGSIZE) {
                pte_t *pte = page_walk(parent->pagetable, heap_block, 0);
                if (pte && (*pte & PTE_V)) {
                    uint64 pa = PTE2PA(*pte);
                    inc_page_ref((void *)pa);
                    uint64 flags = PTE_FLAGS(*pte);
                    flags &= ~PTE_W;
                    flags |= PTE_COW;
                    user_vm_map(child->pagetable, heap_block, PGSIZE, pa, flags | PTE_V);
                    *pte = PA2PTE(pa) | flags | PTE_V;
                    flush_tlb();
                }
            }
            child->mapped_info[HEAP_SEGMENT].npages = parent->mapped_info[HEAP_SEGMENT].npages;
            child->mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;
            child->mapped_info[HEAP_SEGMENT].va = parent->mapped_info[HEAP_SEGMENT].va;
            memcpy((void *)&child->user_heap, (void *)&parent->user_heap, sizeof(parent->user_heap));
            break;
        }
        case CODE_SEGMENT:
            for (int j = 0; j < parent->mapped_info[i].npages; j++) {
                uint64 addr = lookup_pa(parent->pagetable, parent->mapped_info[i].va + j * PGSIZE);
                map_pages(child->pagetable, parent->mapped_info[i].va + j * PGSIZE, PGSIZE, addr,
                          prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));
            }
            child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
            child->mapped_info[child->total_mapped_region].npages = parent->mapped_info[i].npages;
            child->mapped_info[child->total_mapped_region].seg_type = CODE_SEGMENT;
            child->total_mapped_region++;
            break;
        case DATA_SEGMENT:
            for (int j = 0; j < parent->mapped_info[i].npages; j++) {
                void *child_pa = alloc_page();
                uint64 parent_va = parent->mapped_info[i].va + j * PGSIZE;
                void *parent_pa = (void *)lookup_pa(parent->pagetable, parent_va);
                memcpy(child_pa, parent_pa, PGSIZE);
                user_vm_map((pagetable_t)child->pagetable, parent_va, PGSIZE, (uint64)child_pa,
                            prot_to_type(PROT_WRITE | PROT_READ, 1));
            }
            child->mapped_info[child->total_mapped_region].va = parent->mapped_info[i].va;
            child->mapped_info[child->total_mapped_region].npages = parent->mapped_info[i].npages;
            child->mapped_info[child->total_mapped_region].seg_type = DATA_SEGMENT;
            child->total_mapped_region++;
            break;
        }
    }

    child->status = READY;
    child->trapframe->regs.a0 = 0;             // 子进程 fork 返回值为 0
    child->trapframe->regs.tp = target_hartid; // 指定运行的 hart
    child->parent = parent;
    insert_to_specific_ready_queue(child, target_hartid);

    return child->pid; // 父进程获得子进程 pid
}
