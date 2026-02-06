/*
 * routines that scan and load a (host) Executable and Linkable Format (ELF) file
 * into the (emulated) memory.
 */

#include "elf.h"

#include "memlayout.h"
#include "string.h"
#include "riscv.h"
#include "vmm.h"
#include "pmm.h"
#include "spike_interface/spike_utils.h"
#include "vfs.h"

typedef struct elf_info_t {
    struct file *f;
    process *p;
} elf_info;

//
// the implementation of allocater. allocates memory space for later segment loading.
// this allocater is heavily modified @lab2_1, where we do NOT work in bare mode.
//
static void *elf_alloc_mb(elf_ctx *ctx, uint64 elf_pa, uint64 elf_va, uint64 size) {
    elf_info *msg = (elf_info *)ctx->info;
    // we assume that size of proram segment is smaller than a page.
    kassert(size < PGSIZE);
    void *pa = alloc_page();
    if (pa == 0) panic("uvmalloc mem alloc falied\n");

    memset((void *)pa, 0, PGSIZE);
    user_vm_map((pagetable_t)msg->p->pagetable, elf_va, PGSIZE, (uint64)pa,
                prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));

    return pa;
}

//
// actual file reading, using the vfs interface.
//
static uint64 elf_fpread(elf_ctx *ctx, void *dest, uint64 nb, uint64 offset) {
    elf_info *msg = (elf_info *)ctx->info;
    // vfs read interface, defined in vfs.c, is used to read the elf file content.
    vfs_lseek(msg->f, offset, SEEK_SET);
    return vfs_read(msg->f, dest, nb);
}

//
// init elf_ctx, a data structure that loads the elf.
//
elf_status elf_init(elf_ctx *ctx, void *info) {
    ctx->info = info;

    // load the elf header
    if (elf_fpread(ctx, &ctx->ehdr, sizeof(ctx->ehdr), 0) != sizeof(ctx->ehdr)) return EL_EIO;

    // check the signature (magic value) of the elf
    if (ctx->ehdr.magic != ELF_MAGIC) return EL_NOTELF;

    return EL_OK;
}

//
// load the elf segments to memory regions.
//
elf_status elf_load(elf_ctx *ctx) {
    // elf_prog_header structure is defined in kernel/elf.h
    elf_prog_header ph_addr;
    int i, off;

    // traverse the elf program segment headers
    for (i = 0, off = ctx->ehdr.phoff; i < ctx->ehdr.phnum; i++, off += sizeof(ph_addr)) {
        // read segment headers
        if (elf_fpread(ctx, (void *)&ph_addr, sizeof(ph_addr), off) != sizeof(ph_addr)) return EL_EIO;

        if (ph_addr.type != ELF_PROG_LOAD) continue;
        if (ph_addr.memsz < ph_addr.filesz) return EL_ERR;
        if (ph_addr.vaddr + ph_addr.memsz < ph_addr.vaddr) return EL_ERR;

        // allocate memory block before elf loading
        void *dest = elf_alloc_mb(ctx, ph_addr.vaddr, ph_addr.vaddr, ph_addr.memsz);

        // actual loading
        if (elf_fpread(ctx, dest, ph_addr.memsz, ph_addr.off) != ph_addr.memsz)
            return EL_EIO;

        // record the vm region in proc->mapped_info. added @lab3_1
        int j;
        for (j = 0; j < PGSIZE / sizeof(mapped_region); j++) // seek the last mapped region
            if ((process *)(((elf_info *)(ctx->info))->p)->mapped_info[j].va == 0x0) break;

        ((process *)(((elf_info *)(ctx->info))->p))->mapped_info[j].va = ph_addr.vaddr;
        ((process *)(((elf_info *)(ctx->info))->p))->mapped_info[j].npages = 1;

        // SEGMENT_READABLE, SEGMENT_EXECUTABLE, SEGMENT_WRITABLE are defined in kernel/elf.h
        if (ph_addr.flags == (SEGMENT_READABLE | SEGMENT_EXECUTABLE)) {
            ((process *)(((elf_info *)(ctx->info))->p))->mapped_info[j].seg_type = CODE_SEGMENT;
            sprint("CODE_SEGMENT added at mapped info offset:%d\n", j);
        } else if (ph_addr.flags == (SEGMENT_READABLE | SEGMENT_WRITABLE)) {
            ((process *)(((elf_info *)(ctx->info))->p))->mapped_info[j].seg_type = DATA_SEGMENT;
            sprint("DATA_SEGMENT added at mapped info offset:%d\n", j);
        } else
            panic("unknown program segment encountered, segment flag:%d.\n", ph_addr.flags);

        ((process *)(((elf_info *)(ctx->info))->p))->total_mapped_region++;
    }

    return EL_OK;
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
// load the elf of user application, by using the spike file interface.
//
void load_bincode_from_host_elf(process *p) {
    arg_buf arg_bug_msg;

    // retrieve command line arguements
    size_t argc = parse_args(&arg_bug_msg);
    if (!argc) panic("You need to specify the application program!\n");

    sprint("Application: %s\n", arg_bug_msg.argv[0]);

    // elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading process.
    elf_ctx elfloader;
    // elf_info is defined above, used to tie the elf file and its corresponding process.
    elf_info info;

    info.f = vfs_open(arg_bug_msg.argv[0], O_RDONLY);
    info.p = p;
    // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
    if (info.f == NULL) panic("Fail on openning the input application program.\n");

    // init elfloader context. elf_init() is defined above.
    if (elf_init(&elfloader, &info) != EL_OK)
        panic("fail to init elfloader.\n");

    // load elf. elf_load() is defined above.
    if (elf_load(&elfloader) != EL_OK) panic("Fail on loading elf.\n");

    // entry (virtual, also physical in lab1_x) address
    p->trapframe->epc = elfloader.ehdr.entry;

    // close the host spike file
    vfs_close(info.f);

    sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}

void free_proc_user_mem(process *p) {
    for (int i = 0; i < MAX_HEAP_PAGES; i++) {
        p->user_heap.free_pages_address[i] = 0;
    }
    p->user_heap.free_pages_count = 0;
    p->user_heap.heap_top = USER_FREE_ADDRESS_START;
    for (int i = 0; i < p->total_mapped_region; i++) {
        int type = p->mapped_info[i].seg_type;
        if (type == CODE_SEGMENT || type == DATA_SEGMENT || type == HEAP_SEGMENT) {
            if (p->mapped_info[i].npages > 0)
                user_vm_unmap((pagetable_t)p->pagetable, p->mapped_info[i].va, p->mapped_info[i].npages * PGSIZE, 1);
            p->mapped_info[i].va = 0;
            p->mapped_info[i].npages = 0;
            p->mapped_info[i].seg_type = -1;
        }
    }
}

ssize_t do_exec(char *pathname) {
    struct file *f = vfs_open(pathname, O_RDONLY);
    if (f == NULL) {
        sprint("do_exec: Fail on openning the executable file!\n");
        return -1;
    }
    elf_ctx elf_loader;
    elf_info info;
    info.f = f;
    info.p = current;
    if (elf_init(&elf_loader, &info) != EL_OK) {
        sprint("do_exec: fail to init elfloader.\n");
        vfs_close(f);
        return -1;
    }
    free_proc_user_mem(current);
    if (elf_load(&elf_loader) != EL_OK) {
        sprint("do_exec: Fail on loading elf.\n");
        vfs_close(f);
        return -1;
    }

    current->trapframe->epc = elf_loader.ehdr.entry;
    current->trapframe->regs.sp = USER_STACK_TOP;
    vfs_close(f);

    sprint("Application program entry point (virtual address): 0x%lx\n", current->trapframe->epc);
    flush_tlb();
    return 0;
}
