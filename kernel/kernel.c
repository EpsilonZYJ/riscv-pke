/*
 * Supervisor-mode startup codes
 */

#include "riscv.h"
#include "string.h"
#include "elf.h"
#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "sched.h"
#include "memlayout.h"
#include "spike_interface/spike_utils.h"
#include "util/types.h"
#include "vfs.h"
#include "rfs.h"
#include "ramdev.h"
#include "semaphore.h"
#include "sync_utils.h"
#include "debug_config.h"

#include <stdlib.h>

//
// trap_sec_start points to the beginning of S-mode trap segment (i.e., the entry point of
// S-mode trap vector). added @lab2_1
//
extern char trap_sec_start[];

static volatile int s_counter = 0;
static volatile int sched_counter = 0;

//
// turn on paging. added @lab2_1
//
void enable_paging() {
    // write the pointer to kernel page (table) directory into the CSR of "satp".
    write_csr(satp, MAKE_SATP(g_kernel_pagetable));

    // refresh tlb to invalidate its content.
    flush_tlb();
}

static char *get_executable_name(const char *relativepath) {
    int length = strlen(relativepath);
    char *ret = (char *)relativepath + length - 1;
    while (ret >= relativepath && *ret != '/') {
        ret--;
    }
    return ret + 1;
}

typedef union {
    uint64 buf[MAX_CMDLINE_ARGS];
    char *argv[MAX_CMDLINE_ARGS];
} arg_buf;

//
// returns the number (should be 1) of string(s) after PKE kernel in command line.
// and store the string(s) in arg_bug_msg.
//
static size_t parse_args(arg_buf *arg_bug_msg) {
    // HTIFSYS_getmainvars frontend call reads command arguments to (input) *arg_bug_msg
    long r = frontend_syscall(HTIFSYS_getmainvars, (uint64)arg_bug_msg,
                              sizeof(*arg_bug_msg), 0, 0, 0, 0, 0);
    kassert(r == 0);

    size_t pk_argc = arg_bug_msg->buf[0];
    uint64 *pk_argv = &arg_bug_msg->buf[1];

    int arg = 1; // skip the PKE OS kernel string, leave behind only the application name
    for (size_t i = 0; arg + i < pk_argc; i++)
        arg_bug_msg->argv[i] = (char *)(uintptr_t)pk_argv[arg + i];

    // returns the number of strings after PKE kernel in command line
    return pk_argc - arg;
}

//
// load the elf, and construct a "process" (with only a trapframe).
// load_bincode_from_host_elf is defined in elf.c
//
process *load_user_program() {
    uint64 hartid = read_tp();
    assert(hartid < NCPU);

    process *proc;

    proc = alloc_process();

#ifdef SYSTEM_INFO_OUTPUT
    sprint("User application is loading.\n");
#endif

    arg_buf arg_bug_msg;

    // retrieve command line arguements
    size_t argc = parse_args(&arg_bug_msg);
    if (!argc) panic("You need to specify the application program!\n");

    // // Non-primary harts may not have a dedicated argv entry.
    // if (hartid >= argc) {
    //     proc->status = ZOMBIE;
    //     return proc;
    // }

    if (strcmp(get_executable_name(arg_bug_msg.argv[hartid]), "riscv-pke") != 0)
        load_bincode_from_host_elf(proc, arg_bug_msg.argv[hartid]);
    else {
        proc->status = ZOMBIE;
    }

#ifdef MULTICORE_MEM_DEBUG
    sprint("[DEBUG]load_user_program: Loaded user program %s with argc=%d\n", arg_bug_msg.argv[0], argc);
#endif
    return proc;
}

//
// s_start: S-mode entry point of riscv-pke OS kernel.
//
int s_start(void) {
    uint64 hartid = read_tp();
    assert(hartid < NCPU);
#ifdef SYSTEM_INFO_OUTPUT
    sprint("hartid = %d: Enter supervisor mode...\n", hartid);
#endif
    // in the beginning, we use Bare mode (direct) memory mapping as in lab1.
    // but now, we are going to switch to the paging mode @lab2_1.
    // note, the code still works in Bare mode when calling pmm_init() and kern_vm_init().
    write_csr(satp, 0);

    if (hartid == 0) {
        // init phisical memory manager
        pmm_init();

        // build the kernel page table
        kern_vm_init();

        // added @lab3_1
        init_proc_pool();

        // init file system, added @lab4_1
        fs_init();

        init_semaphore_pool();
    }

#ifdef INIT_OUTPUT
    sprint("[DEBUG] hartid = %d: s_start: wait for all NCPU.\n", hartid);
#endif

    sync_barrier(&s_counter, NCPU);

#ifdef INIT_OUTPUT
    sprint("[DEBUG] hartid = %d: s_start: syncronized.\n", hartid);
#endif
    // hart 1 (and other non-zero harts) must also enable paging using the
    // kernel page table that hart 0 has already set up.
    // now, switch to paging mode by turning on paging (SV39)
    enable_paging();

#ifdef INIT_OUTPUT
    // the code now formally works in paging mode, meaning the page table is now in use.
    sprint("hartid = %d: kernel page table is on \n", hartid);
#endif

#ifdef SYSTEM_INFO_OUTPUT
    sprint("hartid = %d: Switch to user mode...\n", hartid);
#endif
    // the application code (elf) is first loaded into memory, and then put into execution
    // added @lab3_1
    process *user_proc = load_user_program();
    if (user_proc->status == ZOMBIE) {
        sprint("hartid = %d: User exit with code:%d.\n", hartid, 0);
        free_process(user_proc);
    } else {
        insert_to_ready_queue(user_proc);
    }
#ifdef INIT_DEBUG
    sprint("[DEBUG] s_start: load user program and insert to ready queue successfully.\n");
#endif

    sync_barrier(&sched_counter, NCPU);

    schedule();

    // we should never reach here.
    return 0;
}
