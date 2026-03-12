#ifndef _SCHED_H_
#define _SCHED_H_

#include "process.h"

// length of a time slice, in number of ticks
#define TIME_SLICE_LEN 2

void insert_to_ready_queue(process *proc);
void insert_to_specific_ready_queue(process *proc, int target_hartid);
void insert_to_block_queue(process **pblock_queue_head, process *proc);
process *wake_from_block_queue(process **pblock_queue_head, process *child_process);
process *remove_from_block_queue(process **pblock_queue_head, process *proc);
process *pick_and_remove_from_block_queue(process **pblock_queue_head);
void schedule();

// fork a child from parent
int do_fork(process *parent);
int do_fork_to_hart(process *parent, int target_hartid);
int do_wait(int64 pid);

#endif
