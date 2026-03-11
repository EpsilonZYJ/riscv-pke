//
// Created by 周煜杰 on 2026/3/10.
//

#include "global.h"
#include "command.h"
#include "parser.h"
#include "util/string.h"
#include "user/user_lib.h"

char *current_dir = NULL;

int exec_supported_command_t(command_t *cur_command) {
    if (strcmp(cur_command->operation, "ls") == 0) {
        if (cur_command->para_num == 0) {
            app_ls(current_dir);
        } else if (cur_command->para_num == 1) {
            app_ls(cur_command->paras->para);
        } else {
            paras_t *para = cur_command->paras;
            while (para != NULL) {
                printu("%s:\n", para->para);
                app_ls(para->para);
                para = para->next;
            }
        }
        return 0;
    } else if (strcmp(cur_command->operation, "cd") == 0) {
        if (cur_command->para_num == 0) {
            app_cd("/");
        } else if (cur_command->para_num == 1) {
            app_cd(cur_command->paras->para);
        } else {
            printu("cd: too many arguments\n");
        }
        return 0;
    } else if (strcmp(cur_command->operation, "cat") == 0) {
        if (cur_command->para_num == 0) {
            printu("cat: missing file operand\n");
        } else if (cur_command->para_num == 1) {
            app_cat(cur_command->paras->para);
        } else {
            paras_t *para = cur_command->paras;
            while (para != NULL) {
                printu("%s:\n", para->para);
                app_cat(para->para);
                para = para->next;
            }
        }
        return 0;
    } else if (strcmp(cur_command->operation, "echo") == 0) {
        if (cur_command->para_num != 2) {
            printu("echo: invalid input!\n");
        } else {
            char *filepath = cur_command->paras->para;
            char *content = cur_command->paras->next->para;
            app_echo(filepath, content);
        }
        return 0;
    } else if (strcmp(cur_command->operation, "cd") == 0) {
        if (cur_command->para_num == 0) {
            app_cd("/");
        } else if (cur_command->para_num == 1) {
            app_cd(cur_command->paras->para);
        } else {
            printu("cd: too many arguments\n");
        }
        return 0;
    } else if (strcmp(cur_command->operation, "pwd") == 0) {
        if (cur_command->para_num == 0) {
            app_pwd();
        } else {
            printu("pwd: too many arguments\n");
        }
        return 0;
    }
    return 1;
}

int exec_command(command_t *command) {
    if (command == NULL) return 0;
    command_t *cur_command = command;
    while (cur_command != NULL && cur_command->op_type != OP_DEAD) {
        if (strcmp(cur_command->operation, "exit") == 0) {
            return 1;
        }
        if (cur_command->op_type == OP_EXEC) {
            if (strcmp(cur_command->operation, "exec") == 0) {
                if (cur_command->para_num == 2) {
                    char *path = cur_command->paras->para;
                    char *para = cur_command->paras->next->para;
                    app_exec(path, para);
                } else if (cur_command->para_num == 1) {
                    char *path = cur_command->paras->para;
                    app_exec(path, "");
                } else {
                    printu("exec: invalid input!\n");
                }
            } else if (startwith(command->operation, "./")) {
                app_exec(cur_command->operation, cur_command->paras == NULL ? "" : cur_command->paras->para);
            } else if (startwith(command->operation, "/")) {
                app_exec(cur_command->operation, cur_command->paras == NULL ? "" : cur_command->paras->para);
            } else {
                int ret = exec_supported_command_t(cur_command);
                if (ret) {
                    printu("Error: unknown command: %s\n", cur_command->operation);
                }
            }
            break; // OP_EXEC只会出现一个命令
        } else if (cur_command->op_type == OP_MULTISTART) {
        }
        cur_command = cur_command->next;
    }
    return 0;
}

int main() {
    current_dir = naive_malloc();
    read_cwd(current_dir);
    int to_exit = 0;
    // Keep one extra byte so parser helpers can safely look past the first '\0'.
    char cmd[101];
    command_t *command = NULL;
    while (!to_exit) {
        printu("~%s $ ", current_dir);
        // getstring() only writes until line end; clear tail to avoid stale tokens.
        memset(cmd, 0, sizeof(cmd));
        getsu(cmd, 100);
        // cmd[strlen(cmd) + 1] = '\0'; // 作为标记
        // parse_cmd(cmd);
        command = build_command(cmd, command);
        if (command == NULL) {
            printu("Error: failed to build command\n");
        } else {
            to_exit = exec_command(command);
        }
        clear_command(command);
    }
    free_command(command);

    naive_free(current_dir);
    exit(0);
    return 0;
}