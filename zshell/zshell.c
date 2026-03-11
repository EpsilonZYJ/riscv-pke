//
// Created by 周煜杰 on 2026/3/10.
//

#include "global.h"
#include "command.h"
#include "parser.h"
#include "util/string.h"
#include "user/user_lib.h"
#include "zshrc_parser.h"

#define MAX_CMD_LEN 100

char *current_dir = NULL;

int exec_supported_command_t(command_t *cur_command);

static int launch_process(char *path, char *para, int wait_child) {
    int pid = fork();
    if (pid < 0) {
        printu("fork failed!\n");
        return -1;
    }
    if (pid == 0) {
        int ret = exec(path, para);
        if (ret == -1) {
            printu("exec failed!\n");
            exit(-1);
        }
        exit(0);
    }
    if (wait_child) {
        wait(pid);
    }
    return 0;
}

static int run_one_command(command_t *cur_command, int wait_child) {
    if (strcmp(cur_command->operation, "exec") == 0) {
        if (cur_command->para_num == 2) {
            char *path = cur_command->paras->para;
            char *para = cur_command->paras->next->para;
            launch_process(path, para, wait_child);
        } else if (cur_command->para_num == 1) {
            char *path = cur_command->paras->para;
            launch_process(path, "", wait_child);
        } else {
            printu("exec: invalid input!\n");
        }
        return 0;
    }

    if (startwith(cur_command->operation, "./") || startwith(cur_command->operation, "/")) {
        launch_process(cur_command->operation,
                       cur_command->paras == NULL ? "" : cur_command->paras->para,
                       wait_child);
        return 0;
    }

    int ret = exec_supported_command_t(cur_command);
    if (ret == 0) {
        return 0;
    }

    const char *alias_val = find_alias(cur_command->operation);
    if (alias_val != NULL) {
        launch_process((char *)alias_val,
                       cur_command->paras == NULL ? "" : cur_command->paras->para,
                       wait_child);
    } else {
        printu("Error: unknown command: %s\n", cur_command->operation);
    }
    return 0;
}

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
            run_one_command(cur_command, 1);
            break; // OP_EXEC只会出现一个命令
        } else if (cur_command->op_type == OP_MULTISTART) {
            run_one_command(cur_command, 0);
        }
        cur_command = cur_command->next;
    }
    return 0;
}

void init_shell() {
    current_dir = naive_malloc();
    read_cwd(current_dir);
    load_zshrc();
}

int main() {
    init_shell();
    int to_exit = 0;
    // Keep one extra byte so parser helpers can safely look past the first '\0'.
    char cmd[MAX_CMD_LEN + 5];
    command_t *command = NULL;

    // 写入命令历史
    int fd_history = open("/.zsh_history", O_RDWR | O_CREAT);
    if (fd_history < 0) {
        printu("Error: failed to open history file\n");
        exit(-1);
    }
    lseek_u(fd_history, 0, SEEK_END);

    while (!to_exit) {
        printu("~%s $ ", current_dir);
        memset(cmd, 0, sizeof(cmd));
        getsu(cmd, MAX_CMD_LEN);

        cmd[strlen(cmd) + 1] = '\0';
        cmd[strlen(cmd)] = '\n';
        // 将命令写入历史文件
        write_u(fd_history, cmd, strlen(cmd));
        cmd[strlen(cmd) - 1] = '\0';

        command = build_command(cmd, command);
        if (command == NULL) {
            printu("Error: failed to build command\n");
        } else {
            to_exit = exec_command(command);
        }
        clear_command(command);
    }

    close(fd_history);
    free_aliases();
    free_command(command);
    naive_free(current_dir);
    exit(0);
    return 0;
}