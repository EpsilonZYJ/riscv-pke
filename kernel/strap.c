/*
 * Utility functions for trap handling in Supervisor mode.
 */

#include "strap.h"
#include "process.h"
#include "riscv.h"
#include "syscall.h"
#include "pmm.h"
#include "vmm.h"
#include "sched.h"
#include "util/functions.h"

#include "spike_interface/spike_utils.h"
#include "util/string.h"

//
// handling the syscalls. will call do_syscall() defined in kernel/syscall.c
//
static void handle_syscall(trapframe *tf) {
    // tf->epc points to the address that our computer will jump to after the trap
    // handling. for a syscall, we should return to the NEXT instruction after its
    // handling. in RV64G, each instruction occupies exactly 32 bits (i.e., 4
    // Bytes)
    tf->epc += 4;

    // (lab1_1): remove the panic call below, and call do_syscall (defined in
    // kernel/syscall.c) to conduct real operations of the kernel side for a
    // syscall. IMPORTANT: return value should be returned to user app, or else,
    // you will encounter problems in later experiments!
    tf->regs.a0 = do_syscall(tf->regs.a0, tf->regs.a1, tf->regs.a2, tf->regs.a3,
                             tf->regs.a4, tf->regs.a5, tf->regs.a6, tf->regs.a7);
}

//
// global variable that store the recorded "ticks". added @lab1_3
static uint64 g_ticks = 0;
//
// added @lab1_3
//
void handle_mtimer_trap() {
    sprint("Ticks %d\n", g_ticks);
    // (lab1_3): increase g_ticks to record this "tick", and then clear the "SIP"
    // field in sip register.
    // hint: use write_csr to disable the SIP_SSIP bit in sip.
    ++g_ticks;
    write_csr(sip, read_csr(sip) & ~SIP_SSIP);
}

//
// the page fault handler. added @lab2_3. parameters:
// sepc: the pc when fault happens;
// stval: the virtual address that causes pagefault when being accessed.
//
void handle_user_page_fault(uint64 mcause, uint64 sepc, uint64 stval) {
    uint64 hartid = read_tp();
    assert(hartid < NCPU);

    sprint("hartid = %d: handle_page_fault: %lx\n", hartid, stval);
    switch (mcause) {
    case CAUSE_STORE_PAGE_FAULT:
        // (lab2_3): implement the operations that solve the page fault to
        // dynamically increase application stack.
        // hint: first allocate a new physical page, and then, maps the new page to the
        // virtual address that causes the page fault.
        {
            pte_t *pte = page_walk(current[hartid]->pagetable, stval, 0);
            if (pte && (*pte & PTE_V) && (*pte & PTE_COW)) {
                uint64 old_pa = PTE2PA(*pte);
                uint64 new_page = old_pa;

                if (get_page_ref((void *)old_pa) > 1) {
                    new_page = (uint64)alloc_page();
                    memcpy((void *)new_page, (void *)old_pa, PGSIZE);
                    dec_page_ref((void *)old_pa);
                }
                uint64 flags = PTE_FLAGS(*pte);
                flags &= ~PTE_COW;                       // clear the copy-on-write bit
                flags |= PTE_W;                          // set the writable bit
                *pte = PA2PTE(new_page) | flags | PTE_V; // update the pte to be writable and not copy-on-write
                flush_tlb();
                break;
            }
        }
        if (stval >= current[hartid]->trapframe->regs.sp - PGSIZE) {
            uint64 new_page = (uint64)alloc_page();
            user_vm_map((pagetable_t)current[hartid]->pagetable, ROUNDDOWN(stval, PGSIZE), PGSIZE,
                        new_page, prot_to_type(PROT_WRITE | PROT_READ, 1));
        } else {
            panic("this address is not available!");
        }
        break;
    default:
        sprint("unknown page fault.\n");
        break;
    }
}

//
// implements round-robin scheduling. added @lab3_3
//
void rrsched() {
    // (lab3_3): implements round-robin scheduling.
    // hint: increase the tick_count member of current process by one, if it is bigger than
    // TIME_SLICE_LEN (means it has consumed its time slice), change its status into READY,
    // place it in the rear of ready queue, and finally schedule next process to run.
    uint64 hartid = read_tp();
    assert(hartid < NCPU);
    if (current[hartid]->tick_count + 1 >= TIME_SLICE_LEN) {
        current[hartid]->tick_count = 0;
        current[hartid]->status = READY;
        insert_to_ready_queue(current[hartid]);
        schedule();
    } else {
        current[hartid]->tick_count++;
    }
}

//
// kernel/smode_trap.S will pass control to smode_trap_handler, when a trap happens
// in S-mode.
//
void smode_trap_handler(void) {
    // make sure we are in User mode before entering the trap handling.
    // we will consider other previous case in lab1_3 (interrupt).
    if ((read_csr(sstatus) & SSTATUS_SPP) != 0)
        panic("usertrap: not from user mode");

    uint64 hartid = read_tp();
    assert(hartid < NCPU);

    assert(current[hartid]);
    // save user process counter.
    current[hartid]->trapframe->epc = read_csr(sepc);

    // if the cause of trap is syscall from user application.
    // read_csr() and CAUSE_USER_ECALL are macros defined in kernel/riscv.h
    uint64 cause = read_csr(scause);

    // use switch-case instead of if-else, as there are many cases since lab2_3.
    switch (cause) {
    case CAUSE_USER_ECALL:
        handle_syscall(current[hartid]->trapframe);
        break;
    case CAUSE_MTIMER_S_TRAP:
        handle_mtimer_trap();
        // invoke round-robin scheduler. added @lab3_3
        rrsched();
        break;
    case CAUSE_STORE_PAGE_FAULT:
    case CAUSE_LOAD_PAGE_FAULT:
        // the address of missing page is stored in stval
        // call handle_user_page_fault to process page faults
        handle_user_page_fault(cause, read_csr(sepc), read_csr(stval));
        break;
    default:
        sprint("smode_trap_handler(): unexpected scause %p\n", read_csr(scause));
        sprint("            sepc=%p stval=%p\n", read_csr(sepc), read_csr(stval));
        panic("unexpected exception happened.\n");
        break;
    }

    // continue (come back to) the execution of current process.
    switch_to(current[hartid]);
}
