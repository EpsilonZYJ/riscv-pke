/*
 * routines that scan and load a (host) Executable and Linkable Format (ELF) file
 * into the (emulated) memory.
 */

#include "elf.h"

#include "debug_config.h"
#include "memlayout.h"
#include "string.h"
#include "riscv.h"
#include "vmm.h"
#include "pmm.h"
#include "vfs.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"

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
    user_vm_map((pagetable_t)msg->p->pagetable, ROUNDDOWN(elf_va, PGSIZE), PGSIZE, (uint64)pa,
                prot_to_type(PROT_WRITE | PROT_READ | PROT_EXEC, 1));

    return (void *)(pa + (elf_va % PGSIZE));
}

//
// actual file reading, using the vfs file interface.
//
static uint64 elf_fpread(elf_ctx *ctx, void *dest, uint64 nb, uint64 offset) {
    elf_info *msg = (elf_info *)ctx->info;
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

//
// load the elf of user application, by using the spike file interface.
//
void load_bincode_from_host_elf(process *p, char *filename) {
    sprint("Application: %s\n", filename);

    // elf loading. elf_ctx is defined in kernel/elf.h, used to track the loading process.
    elf_ctx elfloader;
    // elf_info is defined above, used to tie the elf file and its corresponding process.
    elf_info info;

    info.f = vfs_open(filename, O_RDONLY);
    info.p = p;
    // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
    if (IS_ERR_VALUE(info.f)) panic("Fail on openning the input application program.\n");

    // init elfloader context. elf_init() is defined above.
    if (elf_init(&elfloader, &info) != EL_OK)
        panic("fail to init elfloader.\n");

    // load elf. elf_load() is defined above.
    if (elf_load(&elfloader) != EL_OK) panic("Fail on loading elf.\n");

    // 加载符号表
    elf_status symtab_load_status = elf_load_symbol_table(&elfloader);
    switch (symtab_load_status) {
    case EL_OK: break;
    case EL_EIO:
        sprint("I/O error when loading symbol table from ELF.\n");
        break;
    case EL_ERR:
        sprint("Error when loading symbol table from ELF.\n");
        break;
    default:
        sprint("Unknown error when loading symbol table from ELF.\n");
        break;
    }

    // entry (virtual, also physical in lab1_x) address
    p->trapframe->epc = elfloader.ehdr.entry;

    // close the vfs file
    vfs_close(info.f);

    sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}

ssize_t do_exec(char *command, char *para) {
    char k_command[MAX_PATH_LEN];
    char k_para[MAX_PATH_LEN];

    if (command)
        strcpy(k_command, command);
    else
        k_command[0] = '\0';

    if (para)
        strcpy(k_para, para);
    else
        k_para[0] = '\0';

    // 释放当前进程的用户空间映射，重新加载新的应用程序
    for (int i = 0; i < current->total_mapped_region; i++) {
        int type = current->mapped_info[i].seg_type;
        if (type == CODE_SEGMENT || type == DATA_SEGMENT || type == STACK_SEGMENT || type == HEAP_SEGMENT) {
            if (current->mapped_info[i].npages > 0) {
                const uint64 va = ROUNDDOWN(current->mapped_info[i].va, PGSIZE);
                const uint64 size = current->mapped_info[i].npages * PGSIZE;
                int free_flag = 1;
                if (type == CODE_SEGMENT) free_flag = 0;
                user_vm_unmap(current->pagetable, va, size, free_flag);
                current->mapped_info[i].npages = 0;
                current->mapped_info[i].va = 0;
                current->mapped_info[i].seg_type = 0;
            }
        }
    }
    flush_tlb();

    current->user_heap.mem_rib.alloc_list = NULL;
    current->user_heap.mem_rib.free_list = NULL;
    current->user_heap.mem_rib.alloc = ALLOC_FUNC;
    current->user_heap.mem_rib.free = FREE_FUNC;
    current->user_heap.heap_top = USER_FREE_ADDRESS_START;
    current->user_heap.heap_bottom = USER_FREE_ADDRESS_START;

    void *new_stack_page = alloc_page();
    user_vm_map(current->pagetable, USER_STACK_TOP - PGSIZE, PGSIZE, (uint64)new_stack_page, prot_to_type(PROT_READ | PROT_WRITE, 1));
    current->mapped_info[STACK_SEGMENT].va = USER_STACK_TOP - PGSIZE;
    current->mapped_info[STACK_SEGMENT].npages = 1;
    current->mapped_info[STACK_SEGMENT].seg_type = STACK_SEGMENT;

    // Reinitialize HEAP_SEGMENT after cleanup
    current->mapped_info[HEAP_SEGMENT].va = USER_FREE_ADDRESS_START;
    current->mapped_info[HEAP_SEGMENT].npages = 0;
    current->mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;

    // Reset total_mapped_region to 4 (STACK, CONTEXT, SYSTEM, HEAP are the base segments)
    current->total_mapped_region = 4;

    load_bincode_from_host_elf(current, k_command);

    if (strlen(k_para) > 0) {
        uint64 sp = USER_STACK_TOP;

        int para_len = strlen(k_para) + 1;
        uint64 arg_string_addr = (sp - ((para_len + 7) & ~0x7)); // align to 8 bytes
        uint64 argv_array_addr = (arg_string_addr - 16) & ~0xF;

        if (arg_string_addr > USER_STACK_TOP - PGSIZE && arg_string_addr < USER_STACK_TOP && argv_array_addr > USER_STACK_TOP - PGSIZE && argv_array_addr < USER_STACK_TOP) {
            char *arg_str_pa = (char *)user_va_to_pa((pagetable_t)current->pagetable, (void *)arg_string_addr);
            uint64 *argv_pa = (uint64 *)user_va_to_pa((pagetable_t)current->pagetable, (void *)argv_array_addr);
            if (arg_str_pa != NULL && argv_pa != NULL) {
                strcpy(arg_str_pa, k_para);

                argv_pa[0] = arg_string_addr;
                argv_pa[1] = 0;

                current->trapframe->regs.sp = argv_array_addr;
                current->trapframe->regs.a0 = 1;
                current->trapframe->regs.a1 = argv_array_addr;
            }
        }
    } else {
        current->trapframe->regs.sp = USER_STACK_TOP;
        current->trapframe->regs.a0 = 0;
        current->trapframe->regs.a1 = 0;
    }
    flush_tlb();

    return 0;
}

static symbol_table g_symtab;

/**
 * @brief 从ELF文件中加载符号表到静态内存g_symtab中
 *
 * @param ctx elf上下文
 * @return elf_status 加载状态
 * @version 0.1
 * @author EpsilonZYJ (yujie.zhou05@outlook.com)
 * @date 2025-10-24
 * @copyright Copyright (c) 2025
 */
elf_status elf_load_symbol_table(elf_ctx *ctx) {
    elf_sec_header sh_symtab;
    int i, off;
    int found = 0;
    for (i = 0, off = ctx->ehdr.shoff; i < ctx->ehdr.shnum; i++, off += sizeof(sh_symtab)) {
        // 读取节头
        if (elf_fpread(ctx, (void *)&sh_symtab, sizeof(sh_symtab), off) != sizeof(sh_symtab))
            return EL_EIO;

        if (sh_symtab.sh_type == SHT_SYMTAB) {
            // 找到符号表
            found = 1;
            break;
        }
    }

    if (!found || sh_symtab.sh_size == 0) {
        sprint("No symbol table found in ELF.\n");
        return EL_ERR;
    }

    // 读取符号表内容
    uint64 symtab_addr = sh_symtab.sh_offset;
    uint64 symtab_size = sh_symtab.sh_size;
    uint32 num_symbols = symtab_size / sizeof(elf_symbol_rec);
    if (num_symbols > MAX_SYMBOLS) {
        sprint("Symbol table too large: %u symbols.\n", num_symbols);
        return EL_ERR;
    }
    if (elf_fpread(ctx, (void *)g_symtab.symbols, symtab_size, symtab_addr) != symtab_size)
        return EL_EIO;
    g_symtab.symbol_count = num_symbols;

    // 读取字符串表节头
    uint64 sh_strtab_addr = ctx->ehdr.shoff + (sh_symtab.sh_link * ctx->ehdr.shentsize);
    elf_sec_header sh_strtab;
    if (elf_fpread(ctx, (void *)&sh_strtab, sizeof(sh_strtab), sh_strtab_addr) != sizeof(sh_strtab))
        return EL_EIO;
    if (sh_strtab.sh_type != SHT_STRTAB) {
        sprint("Invalid string table section.\n");
        return EL_ERR;
    }
    if (sh_strtab.sh_size > MAX_STRTAB_SIZE) {
        sprint("String table too large: %lu bytes.\n", sh_strtab.sh_size);
        return EL_ERR;
    }

    // 读取字符串表内容
    if (elf_fpread(ctx, (void *)g_symtab.str_table, sh_strtab.sh_size, sh_strtab.sh_offset) != sh_strtab.sh_size)
        return EL_EIO;
    g_symtab.str_table_size = sh_strtab.sh_size;

#ifdef ELF_LOAD_SYMBOL_TABLE_DEBUG
    for (int i = 0; i < g_symtab.symbol_count; i++) {
        sprint("Loaded symbol: %s \t at 0x%lx, max addr 0x%lx\n", &g_symtab.str_table[g_symtab.symbols[i].st_name], g_symtab.symbols[i].st_value, g_symtab.symbols[i].st_value + g_symtab.symbols[i].st_size);
    }
#endif

    return EL_OK;
}

/**
 * @brief 根据地址查找符号名称
 *
 * @param addr 地址
 * @return const char* 符号名称
 * @version 0.1
 * @author EpsilonZYJ (yujie.zhou05@outlook.com)
 * @date 2025-10-24
 * @copyright Copyright (c) 2025
 */
const char *elf_find_symbol_by_addr(uint64 addr) {
    for (int i = 0; i < g_symtab.symbol_count; i++) {
        elf_symbol_rec *sym = &g_symtab.symbols[i];
        // 如果不是函数类型符号，跳过
        if (ELF_ST_TYPE(sym->st_info) != STT_FUNC) continue;

        if (sym->st_name > g_symtab.str_table_size) {
            sprint("Invalid symbol name offset: %u\n", sym->st_name);
            sprint("ELF inconsistency detected!\n");
            return NULL;
        }

        // 由于调用栈中压入的地址是返回地址，该地址在函数范围内
        // 因此这里不能直接查找是否是某个函数的入口地址
        // 正确方式是查找返回地址是否在函数范围内（函数体内的某个地址）
        if (addr >= sym->st_value && addr < sym->st_value + sym->st_size) {
            return &g_symtab.str_table[sym->st_name];
        }
    }
    return NULL;
}
