/*
 * routines that scan and load a (host) Executable and Linkable Format (ELF) file
 * into the (emulated) memory.
 */

#include "elf.h"
#include "string.h"
#include "riscv.h"
#include "spike_interface/spike_utils.h"

typedef struct elf_info_t {
    spike_file_t *f;
    process *p;
} elf_info;

//
// the implementation of allocater. allocates memory space for later segment loading
//
static void *elf_alloc_mb(elf_ctx *ctx, uint64 elf_pa, uint64 elf_va, uint64 size) {
    // directly returns the virtual address as we are in the Bare mode in lab1_x
    return (void *)elf_va;
}

//
// actual file reading, using the spike file interface.
//
static uint64 elf_fpread(elf_ctx *ctx, void *dest, uint64 nb, uint64 offset) {
    elf_info *msg = (elf_info *)ctx->info;
    // call spike file utility to load the content of elf file into memory.
    // spike_file_pread will read the elf file (msg->f) from offset to memory (indicated by
    // *dest) for nb bytes.
    return spike_file_pread(msg->f, dest, nb, offset);
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
// load the elf segments to memory regions as we are in Bare mode in lab1
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

    info.f = spike_file_open(arg_bug_msg.argv[0], O_RDONLY, 0);
    info.p = p;
    // IS_ERR_VALUE is a macro defined in spike_interface/spike_htif.h
    if (IS_ERR_VALUE(info.f)) panic("Fail on openning the input application program.\n");

    // init elfloader context. elf_init() is defined above.
    if (elf_init(&elfloader, &info) != EL_OK)
        panic("fail to init elfloader.\n");

    // load elf. elf_load() is defined above.
    if (elf_load(&elfloader) != EL_OK) panic("Fail on loading elf.\n");

    // entry (virtual, also physical in lab1_x) address
    p->trapframe->epc = elfloader.ehdr.entry;

    // close the host spike file
    spike_file_close(info.f);

    sprint("Application program entry point (virtual address): 0x%lx\n", p->trapframe->epc);
}

static symbol_table g_symtab;

// 从ELF文件中加载符号表到静态内存g_symtab中
elf_status elf_load_symbol_table(elf_ctx *ctx) {
    elf_sec_header sh_symtab;
    int i, off;
    for (i = 0, off = ctx->ehdr.shoff; i < ctx->ehdr.shnum; i++, off += sizeof(sh_symtab)) {
        // 读取节头
        if (elf_fpread(ctx, (void *)&sh_symtab, sizeof(sh_symtab), off) != sizeof(sh_symtab))
            return EL_EIO;

        if (sh_symtab.sh_type == SHT_SYMTAB) {
            // 找到符号表
            break;
        }
    }

    if (sh_symtab.sh_size == 0) {
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

    return EL_OK;
}

// 根据地址查找符号名称
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
