//
// Created by 周煜杰 on 2026/3/10.
//

#include "global.h"
#include "command.h"
#include "parser.h"
#include "util/string.h"
#include "user/user_lib.h"

char *current_dir = NULL;

/*
 * parse the command and its arguments
 */

void parse_cmd(char *cmd) {
    char *token = get_token(cmd, " ");
    while (token != NULL) {
        if (strcmp(token, "ls") == 0) {
            token = skip_current_token(token, " ");
            if (token == NULL) {
                app_ls(current_dir);
            } else {
                do {
                    app_ls(token);
                    token = strtok(NULL, " ");
                } while (token != NULL);
            }
        } else if (strcmp(token, "cd") == 0) {
            token = skip_current_token(token, " ");
            char *path = token;
            token = skip_current_token(token, " ");
            if (token != NULL) {
                printu("cd: too many arguments\n");
            } else if (path == NULL) {
                app_cd("/");
            } else {
                app_cd(path);
            }
        }
        token = strtok(NULL, " ");
    }
}

void exec_command(command_t *command) {
    if (command == NULL) return;
    command_t *cur_command = command;
    // while (cur_command != NULL &&)
}

int main() {
    current_dir = naive_malloc();
    read_cwd(current_dir);
    int to_exit = 0;
    char cmd[100];
    command_t *command = NULL;
    while (!to_exit) {
        printu("~%s $ ", current_dir);
        getsu(cmd, 100);
        // cmd[strlen(cmd) + 1] = '\0'; // 作为标记
        // parse_cmd(cmd);
        command = build_command(cmd, command);
        if (command == NULL) {
            printu("Error: failed to build command\n");
        } else {
            print_command(command);
        }
        clear_command(command);
    }
    free_command(command);

    naive_free(current_dir);
    exit(0);
    return 0;
}