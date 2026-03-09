#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"
#include "util/string.h"

// #define MTRAP_C_DEBUG

void print_source_code_line(const char *path, int target_line) {
#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]print_source_code_line: path=%s, target_line=%d\n", path, target_line);
#endif
    if (!path) return;
    spike_file_t *f = spike_file_open(path, O_RDONLY, 0);
    if (IS_ERR_VALUE(f)) panic("Source file not found or cannot be opened: %s\n", path);

    char buf[1];
    int cur_line = 0;
    uint64 off = 0;
    while (cur_line < target_line - 1) {
        if (spike_file_pread(f, buf, 1, off++) != 1) break;
        if (buf[0] == '\n') cur_line++;
    }

    if (cur_line == target_line - 1) {
        while (1) {
            if (spike_file_pread(f, buf, 1, off++) != 1) break;
            if (buf[0] == '\n' || buf[0] == '\r') break;
            sprint("%c", buf[0]);
        }
        sprint("\n");
    }
    spike_file_close(f);
}

void print_error_msg(process *proc) {
    if (proc->debugline == NULL || proc->dir == NULL || proc->file == NULL) {
        return;
    }

#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]print_error_msg: read epc\n");
#endif

    uint64 epc = read_csr(mepc);
    int line_num = -1;

    // 查找最近的epc代码映射
    for (int i = 0; i < proc->line_ind; i++) {
        if (proc->line[i].addr <= epc) {
            if (line_num == -1 || proc->line[i].addr > proc->line[line_num].addr) {
                line_num = i;
            }
        }
    }

#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]print_error_msg: found line_num %d\n", line_num);
#endif

    if (line_num == -1) {
        return;
    }

    uint64 file_idx = proc->line[line_num].file;
    if (file_idx >= 64) {
        sprint("print_error_msg: file_idx %d out of bounds\n", file_idx);
        return;
    }
    uint64 dir_idx = proc->file[file_idx].dir;
    if (dir_idx >= 64) {
        sprint("print_error_msg: dir_idx %d out of bounds\n", dir_idx);
        return;
    }
    char *dir_path = proc->dir[dir_idx];
    char *file_name = proc->file[file_idx].file;

    int source_line = proc->line[line_num].line;

    sprint("Runtime error at %s/%s:%d\n", dir_path, file_name, source_line);

    const size_t DIR_PATH_LEN = strlen(dir_path);
    const size_t FILE_NAME_LEN = strlen(file_name);
    char full_path[DIR_PATH_LEN + FILE_NAME_LEN + 2];
    strcpy(full_path, dir_path);
    full_path[DIR_PATH_LEN] = '/';
    strcpy(full_path + DIR_PATH_LEN + 1, file_name);
    print_source_code_line(full_path, source_line);
}

static void handle_instruction_access_fault() {
#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_instruction_access_fault: entered\n");
#endif
    print_error_msg(current);

#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_instruction_access_fault: about to panic\n");
#endif
    panic("Instruction access fault!");
}

static void handle_load_access_fault() {
#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_load_access_fault: entered\n");
#endif

    print_error_msg(current);

#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_load_access_fault: about to panic\n");
#endif

    panic("Load access fault!");
}

static void handle_store_access_fault() {
#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_store_access_fault: entered\n");
#endif

    print_error_msg(current);

#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_store_access_fault: about to panic\n");
#endif

    panic("Store/AMO access fault!");
}

static void handle_illegal_instruction() {
#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_illegal_instruction: entered\n");
#endif

    print_error_msg(current);

#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_illegal_instruction: about to panic\n");
#endif

    panic("Illegal instruction!");
}

static void handle_misaligned_load() {
#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_misaligned_load: entered\n");
#endif

    print_error_msg(current);

#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_misaligned_load: about to panic\n");
#endif

    panic("Misaligned Load!");
}

static void handle_misaligned_store() {
    print_error_msg(current);
    panic("Misaligned AMO!");
}
static void handle_misaligned_store() {
#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_misaligned_store: entered\n");
#endif
    print_error_msg(current);

#ifdef MTRAP_C_DEBUG
    sprint("[DEBUG]handle_misaligned_store: about to panic\n");
#endif

    panic("Misaligned AMO!");
}

// added @lab1_3
static void handle_timer() {
    int cpuid = 0;
    // setup the timer fired at next time (TIMER_INTERVAL from now)
    *(uint64 *)CLINT_MTIMECMP(cpuid) = *(uint64 *)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

    // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
    write_csr(sip, SIP_SSIP);
}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap() {
    uint64 mcause = read_csr(mcause);
    switch (mcause) {
    case CAUSE_MTIMER:
        handle_timer();
        break;
    case CAUSE_FETCH_ACCESS:
        handle_instruction_access_fault();
        break;
    case CAUSE_LOAD_ACCESS:
        handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
        handle_store_access_fault();
        break;
    case CAUSE_ILLEGAL_INSTRUCTION:
        // (lab1_2): call handle_illegal_instruction to implement illegal instruction
        // interception, and finish lab1_2.
        handle_illegal_instruction();
        break;
    case CAUSE_MISALIGNED_LOAD:
        handle_misaligned_load();
        break;
    case CAUSE_MISALIGNED_STORE:
        handle_misaligned_store();
        break;

    default:
        sprint("machine trap(): unexpected mscause %p\n", mcause);
        sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
        panic("unexpected exception happened in M-mode.\n");
        break;
    }
}
