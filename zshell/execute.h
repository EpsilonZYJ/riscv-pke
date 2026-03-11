//
// Created by 周煜杰 on 2026/3/11.
//

#ifndef RISCV_PKE_EXECUTE_H
#define RISCV_PKE_EXECUTE_H

#include "global.h"
#include "parser.h"

int launch_process(char *path, char *para, int wait_child);
int run_one_command(command_t *cur_command, int wait_child);
int exec_supported_command_t(command_t *cur_command);
int exec_command(command_t *command);

#endif // RISCV_PKE_EXECUTE_H
