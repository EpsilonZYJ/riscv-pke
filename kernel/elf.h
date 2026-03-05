#ifndef _ELF_H_
#define _ELF_H_

#include "util/types.h"
#include "process.h"

#define MAX_CMDLINE_ARGS 64

// Section types
#define SHT_NULL 0     // Section header table entry unused
#define SHT_PROGBITS 1 // Program data
#define SHT_SYMTAB 2   // Symbol table
#define SHT_STRTAB 3   // String table
#define SHT_RELA 4     // Relocation entries with addends
#define SHT_HASH 5     // Symbol hash table
#define SHT_DYNAMIC 6  // Dynamic linking information
#define SHT_NOTE 7     // Notes
#define SHT_NOBITS 8   // Program space with no data (bss)
#define SHT_REL 9      // Relocation entries, no addends
#define SHT_SHLIB 10   // Reserved
#define SHT_DYNSYM 11  // Dynamic linker symbol table

// Symbol bindings
#define STB_LOCAL 0  // Local symbol
#define STB_GLOBAL 1 // Global symbol
#define STB_WEAK 2   // Weak symbol

// Symbol types
#define STT_NOTYPE 0  // Symbol type is unspecified
#define STT_OBJECT 1  // Symbol is a data object
#define STT_FUNC 2    // Symbol is a code object
#define STT_SECTION 3 // Symbol associated with a section
#define STT_FILE 4    // Symbol's name is file name

// Extract symbol binding from st_info
#define ELF_ST_BIND(i) ((i) >> 4)
// Extract symbol type from st_info
#define ELF_ST_TYPE(i) ((i) & 0xf)

// 静态内存存储符号表
#define MAX_SYMBOLS 1024
#define MAX_STRTAB_SIZE 8192

// elf header structure
typedef struct elf_header_t {
    uint32 magic;
    uint8 elf[12];
    uint16 type;      /* Object file type */
    uint16 machine;   /* Architecture */
    uint32 version;   /* Object file version */
    uint64 entry;     /* Entry point virtual address */
    uint64 phoff;     /* Program header table file offset */
    uint64 shoff;     /* Section header table file offset */
    uint32 flags;     /* Processor-specific flags */
    uint16 ehsize;    /* ELF header size in bytes */
    uint16 phentsize; /* Program header table entry size */
    uint16 phnum;     /* Program header table entry count */
    uint16 shentsize; /* Section header table entry size */
    uint16 shnum;     /* Section header table entry count */
    uint16 shstrndx;  /* Section header string table index */
} elf_header;

// segment types, attributes of elf_prog_header_t.flags
#define SEGMENT_READABLE 0x4
#define SEGMENT_EXECUTABLE 0x1
#define SEGMENT_WRITABLE 0x2

// Program segment header.
typedef struct elf_prog_header_t {
    uint32 type;   /* Segment type */
    uint32 flags;  /* Segment flags */
    uint64 off;    /* Segment file offset */
    uint64 vaddr;  /* Segment virtual address */
    uint64 paddr;  /* Segment physical address */
    uint64 filesz; /* Segment size in file */
    uint64 memsz;  /* Segment size in memory */
    uint64 align;  /* Segment alignment */
} elf_prog_header;

#define ELF_MAGIC 0x464C457FU // "\x7FELF" in little endian
#define ELF_PROG_LOAD 1

typedef enum elf_status_t {
    EL_OK = 0,
    EL_EIO,
    EL_ENOMEM,
    EL_NOTELF,
    EL_ERR,
} elf_status;

typedef struct elf_ctx_t {
    void *info;
    elf_header ehdr;
} elf_ctx;

typedef struct elf_symbol_rec {
    // st_name is the offset in the symbol string table
    uint32 st_name;         // Symbol name (string tbl index)
    unsigned char st_info;  // Symbol type and binding
    unsigned char st_other; // Symbol visibility
    uint16 st_shndx;        // Section index
    uint64 st_value;        // Symbol value
    uint64 st_size;         // Symbol size
} elf_symbol_rec;

typedef struct elf_sec_header_t {
    uint32 sh_name;      /* Section name (string tbl index) */
    uint32 sh_type;      /* Section type */
    uint64 sh_flags;     /* Section flags */
    uint64 sh_addr;      /* Section virtual addr at execution */
    uint64 sh_offset;    /* Section file offset */
    uint64 sh_size;      /* Section size in bytes */
    uint32 sh_link;      /* Link to another section */
    uint32 sh_info;      /* Additional section information */
    uint64 sh_addralign; /* Section alignment */
    uint64 sh_entsize;   /* Entry size if section holds table */
} elf_sec_header;

typedef struct symbol_table_t {
    elf_symbol_rec symbols[MAX_SYMBOLS]; // array of symbols
    uint32 symbol_count;                 // number of symbols
    char str_table[MAX_STRTAB_SIZE];     // string table
    uint32 str_table_size;               // size of string table
} symbol_table;

elf_status elf_init(elf_ctx *ctx, void *info);
elf_status elf_load(elf_ctx *ctx);

void load_bincode_from_host_elf(process *p, char *filename);

ssize_t do_exec(char *command, char *para);

elf_status elf_load_symbol_table(elf_ctx *ctx);
const char *elf_find_symbol_by_addr(uint64 addr);

#endif
