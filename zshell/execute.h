//
// Created by 周煜杰 on 2026/3/11.
//

#ifndef RISCV_PKE_EXECUTE_H
#define RISCV_PKE_EXECUTE_H

#include "global.h"
#include "parser.h"

int launch_process(char *path, char *para, int wait_child, int *wait_count);
int launch_process_to_hart(char *path, char *para, int target_hartid, int wait_child,
						   int *wait_count);
int run_one_command(command_t *cur_command, int wait_child, int *wait_count);
int run_one_command_multicore(command_t *cur_command, int target_hartid, int wait_child,
							  int *wait_count);
int exec_supported_command_t(command_t *cur_command);
int exec_command(command_t *command);

#endif // RISCV_PKE_EXECUTE_H
