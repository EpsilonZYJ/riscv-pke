//
// Created by 周煜杰 on 2026/3/10.
//

#include "global.h"
#include "command.h"
#include "parser.h"
#include "util/string.h"
#include "user/user_lib.h"
#include "zshrc_parser.h"
#include "execute.h"

#define MAX_CMD_LEN 100

char *current_dir = NULL;

int exec_supported_command_t(command_t *cur_command);

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