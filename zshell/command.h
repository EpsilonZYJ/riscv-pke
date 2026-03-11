//
// Created by 周煜杰 on 2026/3/11.
//

#ifndef RISCV_PKE_COMMAND_H
#define RISCV_PKE_COMMAND_H

#include "global.h"

void app_pwd();
void app_cd(const char *path);
void app_cat(const char *filename);
void app_echo(const char *filepath, char *content);
void app_exec(char *path, char *para);
void app_ls(const char *path);
void app_mkdir(const char *path);
void app_touch(const char *path);

#endif // RISCV_PKE_COMMAND_H
