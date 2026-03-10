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
#include "config.h"
#include "debug_config.h"

// 64KB, aligned
static uint64 debug_line_buf[NCPU][MAX_DEBUG_LINE_SIZE / sizeof(uint64)];

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
    // This helper allocates one page and returns the offset pointer in that page.
    // Callers should pass size <= PGSIZE.
    kassert(size <= PGSIZE);
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

// leb128 (little-endian base 128) is a variable-length
// compression algoritm in DWARF
void read_uleb128(uint64 *out, char **off) {
    uint64 value = 0;
    int shift = 0;
    uint8 b;
    for (;;) {
        b = *(uint8 *)(*off);
        (*off)++;
        value |= ((uint64)b & 0x7F) << shift;
        shift += 7;
        if ((b & 0x80) == 0) break;
    }
    if (out) *out = value;
}
void read_sleb128(int64 *out, char **off) {
    int64 value = 0;
    int shift = 0;
    uint8 b;
    for (;;) {
        b = *(uint8 *)(*off);
        (*off)++;
        value |= ((uint64_t)b & 0x7F) << shift;
        shift += 7;
        if ((b & 0x80) == 0) break;
    }
    if (shift < 64 && (b & 0x40)) value |= -(1 << shift);
    if (out) *out = value;
}
// Since reading below types through pointer cast requires aligned address,
// so we can only read them byte by byte
void read_uint64(uint64 *out, char **off) {
    *out = 0;
    for (int i = 0; i < 8; i++) {
        *out |= (uint64)(**off) << (i << 3);
        (*off)++;
    }
}
void read_uint32(uint32 *out, char **off) {
    *out = 0;
    for (int i = 0; i < 4; i++) {
        *out |= (uint32)(**off) << (i << 3);
        (*off)++;
    }
}
void read_uint16(uint16 *out, char **off) {
    *out = 0;
    for (int i = 0; i < 2; i++) {
        *out |= (uint16)(**off) << (i << 3);
        (*off)++;
    }
}

/*
 * analyzis the data in the debug_line section
 *
 * the function needs 3 parameters: elf context, data in the debug_line section
 * and length of debug_line section
 *
 * make 3 arrays:
 * "process->dir" stores all directory paths of code files
 * "process->file" stores all code file names of code files and their directory path index of array "dir"
 * "process->line" stores all relationships map instruction addresses to code line numbers
 * and their code file name index of array "file"
 */
void make_addr_line(elf_ctx *ctx, char *debug_line, uint64 length) {
    process *p = ((elf_info *)ctx->info)->p;
    p->debugline = debug_line;
    // directory name char pointer array
    p->dir = (char **)((((uint64)debug_line + length + 7) >> 3) << 3);
    int dir_ind = 0, dir_base;
    // file name char pointer array
    p->file = (code_file *)(p->dir + 64);
    int file_ind = 0, file_base;
    // table array
    p->line = (addr_line *)(p->file + 64);
    p->line_ind = 0;
    char *off = debug_line;
    while (off < debug_line + length) { // iterate each compilation unit(CU)
        debug_header *dh = (debug_header *)off;
        off += sizeof(debug_header);
        dir_base = dir_ind;
        file_base = file_ind;
        // get directory name char pointer in this CU
        while (*off != 0) {
            p->dir[dir_ind++] = off;
            while (*off != 0) off++;
            off++;
        }
        off++;
        // get file name char pointer in this CU
        while (*off != 0) {
            p->file[file_ind].file = off;
            while (*off != 0) off++;
            off++;
            uint64 dir;
            read_uleb128(&dir, &off);
            p->file[file_ind++].dir = dir - 1 + dir_base;
            read_uleb128(NULL, &off);
            read_uleb128(NULL, &off);
        }
        off++;
        addr_line regs;
        regs.addr = 0;
        regs.file = 1;
        regs.line = 1;
        // simulate the state machine op code
        for (;;) {
            uint8 op = *(off++);
            switch (op) {
            case 0: // Extended Opcodes
                read_uleb128(NULL, &off);
                op = *(off++);
                switch (op) {
                case 1: // DW_LNE_end_sequence
                    if (p->line_ind > 0 && p->line[p->line_ind - 1].addr == regs.addr) p->line_ind--;
                    p->line[p->line_ind] = regs;
                    p->line[p->line_ind].file += file_base - 1;
                    p->line_ind++;
                    goto endop;
                case 2: // DW_LNE_set_address
                    read_uint64(&regs.addr, &off);
                    break;
                // ignore DW_LNE_define_file
                case 4: // DW_LNE_set_discriminator
                    read_uleb128(NULL, &off);
                    break;
                }
                break;
            case 1: // DW_LNS_copy
                if (p->line_ind > 0 && p->line[p->line_ind - 1].addr == regs.addr) p->line_ind--;
                p->line[p->line_ind] = regs;
                p->line[p->line_ind].file += file_base - 1;
                p->line_ind++;
                break;
            case 2: { // DW_LNS_advance_pc
                uint64 delta;
                read_uleb128(&delta, &off);
                regs.addr += delta * dh->min_instruction_length;
                break;
            }
            case 3: { // DW_LNS_advance_line
                int64 delta;
                read_sleb128(&delta, &off);
                regs.line += delta;
                break;
            }
            case 4: // DW_LNS_set_file
                read_uleb128(&regs.file, &off);
                break;
            case 5: // DW_LNS_set_column
                read_uleb128(NULL, &off);
                break;
            case 6: // DW_LNS_negate_stmt
            case 7: // DW_LNS_set_basic_block
                break;
            case 8: { // DW_LNS_const_add_pc
                int adjust = 255 - dh->opcode_base;
                int delta = (adjust / dh->line_range) * dh->min_instruction_length;
                regs.addr += delta;
                break;
            }
            case 9: { // DW_LNS_fixed_advanced_pc
                uint16 delta;
                read_uint16(&delta, &off);
                regs.addr += delta;
                break;
            }
            default: { // Special Opcodes
                int adjust = op - dh->opcode_base;
                int addr_delta = (adjust / dh->line_range) * dh->min_instruction_length;
                int line_delta = dh->line_base + (adjust % dh->line_range);
                regs.addr += addr_delta;
                regs.line += line_delta;
                if (p->line_ind > 0 && p->line[p->line_ind - 1].addr == regs.addr) p->line_ind--;
                p->line[p->line_ind] = regs;
                p->line[p->line_ind].file += file_base - 1;
                p->line_ind++;
                break;
            }
            }
        }
    endop:;
    }
    // for (int i = 0; i < p->line_ind; i++)
    //     sprint("%p %d %d\n", p->line[i].addr, p->line[i].line, p->line[i].file);
}

//
// load the elf segments to memory regions.
//
elf_status elf_load(elf_ctx *ctx) {
    // elf_prog_header structure is defined in kernel/elf.h
    elf_prog_header ph_addr;
    elf_info *msg = (elf_info *)ctx->info;
    int i, off;

    // traverse the elf program segment headers
    for (i = 0, off = ctx->ehdr.phoff; i < ctx->ehdr.phnum; i++, off += sizeof(ph_addr)) {
        // read segment headers
        if (elf_fpread(ctx, (void *)&ph_addr, sizeof(ph_addr), off) != sizeof(ph_addr)) return EL_EIO;

        if (ph_addr.type != ELF_PROG_LOAD) continue;
        if (ph_addr.memsz < ph_addr.filesz) return EL_ERR;
        if (ph_addr.vaddr + ph_addr.memsz < ph_addr.vaddr) return EL_ERR;

        // map the whole segment page by page to support multi-page ELF segments.
        uint64 seg_start = ROUNDDOWN(ph_addr.vaddr, PGSIZE);
        uint64 seg_end = ROUNDUP(ph_addr.vaddr + ph_addr.memsz, PGSIZE);
        int prot = 0;
        if (ph_addr.flags & SEGMENT_READABLE) prot |= PROT_READ;
        if (ph_addr.flags & SEGMENT_WRITABLE) prot |= PROT_WRITE;
        if (ph_addr.flags & SEGMENT_EXECUTABLE) prot |= PROT_EXEC;

        for (uint64 va = seg_start; va < seg_end; va += PGSIZE) {
            void *pa = alloc_page();
            if (pa == 0) panic("uvmalloc mem alloc falied\n");
            memset(pa, 0, PGSIZE);
            user_vm_map((pagetable_t)msg->p->pagetable, va, PGSIZE, (uint64)pa,
                        prot_to_type(prot, 1));
        }

        // load file-backed bytes; bss tail remains zero-initialized.
        uint64 remaining = ph_addr.filesz;
        uint64 cur_va = ph_addr.vaddr;
        uint64 cur_off = ph_addr.off;
        while (remaining > 0) {
            void *dest = user_va_to_pa((pagetable_t)msg->p->pagetable, (void *)cur_va);
            if (dest == 0) return EL_ERR;

            uint64 page_left = PGSIZE - (cur_va % PGSIZE);
            uint64 chunk = remaining < page_left ? remaining : page_left;
            if (elf_fpread(ctx, dest, chunk, cur_off) != chunk) return EL_EIO;

            remaining -= chunk;
            cur_va += chunk;
            cur_off += chunk;
        }

        // record the vm region in proc->mapped_info. added @lab3_1
        int j;
        for (j = 0; j < PGSIZE / sizeof(mapped_region); j++) // seek the last mapped region
            if ((process *)(((elf_info *)(ctx->info))->p)->mapped_info[j].va == 0x0) break;

        ((process *)(((elf_info *)(ctx->info))->p))->mapped_info[j].va = ph_addr.vaddr;
        ((process *)(((elf_info *)(ctx->info))->p))->mapped_info[j].npages =
            (seg_end - seg_start) / PGSIZE;

        // SEGMENT_READABLE, SEGMENT_EXECUTABLE, SEGMENT_WRITABLE are defined in kernel/elf.h
        if (ph_addr.flags == (SEGMENT_READABLE | SEGMENT_EXECUTABLE)) {
            ((process *)(((elf_info *)(ctx->info))->p))->mapped_info[j].seg_type = CODE_SEGMENT;
#ifdef INIT_OUTPUT
            sprint("CODE_SEGMENT added at mapped info offset:%d\n", j);
#endif
        } else if (ph_addr.flags == (SEGMENT_READABLE | SEGMENT_WRITABLE)) {
            ((process *)(((elf_info *)(ctx->info))->p))->mapped_info[j].seg_type = DATA_SEGMENT;
#ifdef INIT_OUTPUT
            sprint("DATA_SEGMENT added at mapped info offset:%d\n", j);
#endif
        } else
            panic("unknown program segment encountered, segment flag:%d.\n", ph_addr.flags);

        ((process *)(((elf_info *)(ctx->info))->p))->total_mapped_region++;
    }

    return EL_OK;
}

/**
 * @brief 从elf文件中加载.debug_line节
 * @param ctx elf上下文
 * @param pdebug_line 存储.debug_line节内容的指针
 * @param plength 存储.debug_line节长度的指针
 * @return 加载状态
 */
elf_status load_debug_line_section_header(elf_ctx *ctx, char **pdebug_line, uint64 *plength, elf_sect_header *psect_header) {
    uint64 hartid = read_tp();
    psect_header->size = 0;

    // 1. Get String Table Section Header
    elf_sect_header shstr_header;
    uint64 shstr_off = ctx->ehdr.shoff + ctx->ehdr.shstrndx * sizeof(elf_sect_header);
    if (elf_fpread(ctx, &shstr_header, sizeof(shstr_header), shstr_off) != sizeof(shstr_header))
        return EL_EIO;

    // 2. Load String Table
    if (shstr_header.size > sizeof(debug_line_buf[hartid])) return EL_ENOMEM;
    if (elf_fpread(ctx, (void *)debug_line_buf[hartid], shstr_header.size, shstr_header.offset) != shstr_header.size)
        return EL_EIO;

    char *shstrtab = (char *)debug_line_buf[hartid];

    // 3. Iterate sections to find .debug_line
    int i, off;
    int found = 0;
    for (i = 0, off = ctx->ehdr.shoff; i < ctx->ehdr.shnum; i++, off += sizeof(*psect_header)) {
        if (elf_fpread(ctx, (void *)psect_header, sizeof(*psect_header), off) != sizeof(*psect_header))
            return EL_EIO;

        if (psect_header->type == SHT_PROGBITS) {
            char *name = shstrtab + psect_header->name;
            if (strcmp(name, ".debug_line") == 0) {
                found = 1;
                break;
            }
        }
    }

    if (!found) return EL_ERR; // Not found

    // 4. Load .debug_line content (overwriting shstrtab)
    if (psect_header->size > sizeof(debug_line_buf[hartid])) return EL_ENOMEM;
    if (elf_fpread(ctx, (void *)debug_line_buf[hartid], psect_header->size, psect_header->offset) != psect_header->size) {
        return EL_EIO;
    }

    // 读取.debug_line节内容
    *pdebug_line = (char *)debug_line_buf[hartid];
    *plength = psect_header->size;
    return EL_OK;
}

// /**
//  * @brief 加载.debug_line节
//  * @param ctx elf上下文
//  * @param sect_header .debug_line节头
//  * @return 加载状态
//  */
elf_status load_debug_line_section(elf_ctx *ctx, elf_sect_header sect_header) {
    uint64 hartid = read_tp();
    assert(hartid < NCPU && hartid >= 0);
    if (sect_header.size > sizeof(debug_line_buf[hartid])) {
#ifdef ELF_C_DEBUG
        sprint("[DEBUG]load_debug_line_section: Debug line section too large: %d bytes\n", sect_header.size);
#endif

        return EL_ENOMEM;
    }
    if (elf_fpread(ctx, (void *)debug_line_buf[hartid], sect_header.size, sect_header.offset) != sect_header.size) {
        return EL_EIO;
    }
    return EL_OK;
}

//
// load the elf of user application, by using the spike file interface.
//
void load_bincode_from_host_elf(process *p, char *filename) {
    uint64 hartid = read_tp();
#ifdef SYSTEM_INFO_OUTPUT
    sprint("hartid = %d: Application: %s\n", hartid, filename);
#endif

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

    // load .debug_line section header and its content
    uint64 debug_line_length;
    elf_sect_header debug_line_sect_header;
    elf_status debug_line_status = load_debug_line_section_header(&elfloader, &p->debugline, &debug_line_length, &debug_line_sect_header);
    if (debug_line_status == EL_OK) {
        make_addr_line(&elfloader, p->debugline, debug_line_length);
    } else {
        sprint("Program do not support debug line!\n");
    }

    // entry (virtual, also physical in lab1_x) address
    p->trapframe->epc = elfloader.ehdr.entry;

    // close the vfs file
    vfs_close(info.f);

#ifdef SYSTEM_INFO_OUTPUT
    sprint("hartid = %d: Application program entry point (virtual address): 0x%lx\n", hartid, p->trapframe->epc);
#endif
}

ssize_t do_exec(char *command, char *para) {
    uint64 hartid = read_tp();
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
    for (int i = 0; i < current[hartid]->total_mapped_region; i++) {
        int type = current[hartid]->mapped_info[i].seg_type;
        if (type == CODE_SEGMENT || type == DATA_SEGMENT || type == STACK_SEGMENT || type == HEAP_SEGMENT) {
            if (current[hartid]->mapped_info[i].npages > 0) {
                const uint64 va = ROUNDDOWN(current[hartid]->mapped_info[i].va, PGSIZE);
                const uint64 size = current[hartid]->mapped_info[i].npages * PGSIZE;
                int free_flag = 1;
                if (type == CODE_SEGMENT) free_flag = 0;
                user_vm_unmap(current[hartid]->pagetable, va, size, free_flag);
                current[hartid]->mapped_info[i].npages = 0;
                current[hartid]->mapped_info[i].va = 0;
                current[hartid]->mapped_info[i].seg_type = 0;
            }
        }
    }
    flush_tlb();

    current[hartid]->user_heap.mem_rib.alloc_list = NULL;
    current[hartid]->user_heap.mem_rib.free_list = NULL;
    current[hartid]->user_heap.mem_rib.alloc = ALLOC_FUNC;
    current[hartid]->user_heap.mem_rib.free = FREE_FUNC;
    current[hartid]->user_heap.heap_top = USER_FREE_ADDRESS_START;
    current[hartid]->user_heap.heap_bottom = USER_FREE_ADDRESS_START;

    void *new_stack_page = alloc_page();
    user_vm_map(current[hartid]->pagetable, USER_STACK_TOP - PGSIZE, PGSIZE, (uint64)new_stack_page, prot_to_type(PROT_READ | PROT_WRITE, 1));
    current[hartid]->mapped_info[STACK_SEGMENT].va = USER_STACK_TOP - PGSIZE;
    current[hartid]->mapped_info[STACK_SEGMENT].npages = 1;
    current[hartid]->mapped_info[STACK_SEGMENT].seg_type = STACK_SEGMENT;

    // Reinitialize HEAP_SEGMENT after cleanup
    current[hartid]->mapped_info[HEAP_SEGMENT].va = USER_FREE_ADDRESS_START;
    current[hartid]->mapped_info[HEAP_SEGMENT].npages = 0;
    current[hartid]->mapped_info[HEAP_SEGMENT].seg_type = HEAP_SEGMENT;

    // Reset total_mapped_region to 4 (STACK, CONTEXT, SYSTEM, HEAP are the base segments)
    current[hartid]->total_mapped_region = 4;

    load_bincode_from_host_elf(current[hartid], k_command);

    if (strlen(k_para) > 0) {
        uint64 sp = USER_STACK_TOP;

        int para_len = strlen(k_para) + 1;
        uint64 arg_string_addr = (sp - ((para_len + 7) & ~0x7)); // align to 8 bytes
        uint64 argv_array_addr = (arg_string_addr - 16) & ~0xF;

        if (arg_string_addr > USER_STACK_TOP - PGSIZE && arg_string_addr < USER_STACK_TOP && argv_array_addr > USER_STACK_TOP - PGSIZE && argv_array_addr < USER_STACK_TOP) {
            char *arg_str_pa = (char *)user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)arg_string_addr);
            uint64 *argv_pa = (uint64 *)user_va_to_pa((pagetable_t)current[hartid]->pagetable, (void *)argv_array_addr);
            if (arg_str_pa != NULL && argv_pa != NULL) {
                strcpy(arg_str_pa, k_para);

                argv_pa[0] = arg_string_addr;
                argv_pa[1] = 0;

                current[hartid]->trapframe->regs.sp = argv_array_addr;
                current[hartid]->trapframe->regs.a0 = 1;
                current[hartid]->trapframe->regs.a1 = argv_array_addr;
            }
        }
    } else {
        current[hartid]->trapframe->regs.sp = USER_STACK_TOP;
        current[hartid]->trapframe->regs.a0 = 0;
        current[hartid]->trapframe->regs.a1 = 0;
    }
    flush_tlb();

    return 0;
}

static symbol_table g_symtab[NCPU];

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
    uint64 hartid = read_tp();
    assert(hartid < NCPU);
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
    if (elf_fpread(ctx, (void *)g_symtab[hartid].symbols, symtab_size, symtab_addr) != symtab_size)
        return EL_EIO;
    g_symtab[hartid].symbol_count = num_symbols;

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
    if (elf_fpread(ctx, (void *)g_symtab[hartid].str_table, sh_strtab.sh_size, sh_strtab.sh_offset) != sh_strtab.sh_size)
        return EL_EIO;
    g_symtab[hartid].str_table_size = sh_strtab.sh_size;

#ifdef ELF_LOAD_SYMBOL_TABLE_DEBUG
    for (int i = 0; i < g_symtab[hartid].symbol_count; i++) {
        sprint("Loaded symbol: %s \t at 0x%lx, max addr 0x%lx\n", &g_symtab[hartid].str_table[g_symtab[hartid].symbols[i].st_name], g_symtab[hartid].symbols[i].st_value, g_symtab[hartid].symbols[i].st_value + g_symtab[hartid].symbols[i].st_size);
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
    uint64 hartid = read_tp();
    for (int i = 0; i < g_symtab[hartid].symbol_count; i++) {
        elf_symbol_rec *sym = &g_symtab[hartid].symbols[i];
        // 如果不是函数类型符号，跳过
        if (ELF_ST_TYPE(sym->st_info) != STT_FUNC) continue;

        if (sym->st_name > g_symtab[hartid].str_table_size) {
            sprint("Invalid symbol name offset: %u\n", sym->st_name);
            sprint("ELF inconsistency detected!\n");
            return NULL;
        }

        // 由于调用栈中压入的地址是返回地址，该地址在函数范围内
        // 因此这里不能直接查找是否是某个函数的入口地址
        // 正确方式是查找返回地址是否在函数范围内（函数体内的某个地址）
        if (addr >= sym->st_value && addr < sym->st_value + sym->st_size) {
            return &g_symtab[hartid].str_table[sym->st_name];
        }
    }
    return NULL;
}
