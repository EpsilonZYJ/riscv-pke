//
// Created by 周煜杰 on 2026/3/11.
//

#ifndef RISCV_PKE_PARSER_H
#define RISCV_PKE_PARSER_H

#include "global.h"
#include "util/string.h"

enum operation_t {
    OP_DEAD,
    OP_EXEC,
    OP_PIPLINE,
    OP_MULTISTART,
    OP_MULTICORE,
};

typedef struct paras_t {
    char *para;
    struct paras_t *next;
} paras_t;

typedef struct command_t {
    char *operation;
    paras_t *paras;
    int para_num;
    enum operation_t op_type;
    struct command_t *next;
} command_t;

char *get_token(char *input, char *delim);
char *skip_current_token(char *input, char *delim);
command_t *build_command(char *cmdline, command_t *command);
void free_paras(command_t *command);
void free_command(command_t *command);
void print_command(command_t *command);
void clear_command(command_t *command);
#endif // RISCV_PKE_PARSER_H
