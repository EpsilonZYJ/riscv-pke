//
// Created by 周煜杰 on 2026/3/10.
//

#include "user/user_lib.h"
#include "util/types.h"
#include "util/string.h"

char *current_dir = NULL;

typedef struct paras_t {
    char *para;
    struct paras_t *next;
} paras_t;

struct command_t {
    char *operation;
    paras_t *paras;
} command_t;

/*
 * a simple shell program, which supports the following commands:
 */
void app_pwd() {
    char path[50];
    read_cwd(path);
    printu("%s\n", path);
}

void app_cd(const char *path) {
    if (change_cwd(path) != 0)
        printu("cd: no such file or directory: %s\n", path);
}

void app_cat(const char *filename) {
    // FIXME: we should read the file in a loop until EOF, but currently we just read it once
    int MAXBUF = 512;
    char buf[MAXBUF];
    int fd = open(filename, O_RDWR);
    read_u(fd, buf, MAXBUF);
    printu("%s\n", buf);
    close(fd);
}

void app_echo(const char *filepath, char *content) {
    int fd = open(filepath, O_RDWR | O_CREAT);
    write_u(fd, content, strlen(content));
    close(fd);
}

void app_exec(char *path, char *para) {
    int ret = exec(path, para);
}

void app_ls(const char *path) {
    int dir_fd = opendir_u(path);
    if (dir_fd == -1) {
        printu("ls: %s: No such file or directory\n", path);
        return;
    }
    struct dir dir;
    int width = 20;
    int count = 0;
    while (readdir_u(dir_fd, &dir) == 0) {
        // we do not have %ms :(
        char name[width + 1];
        memset(name, ' ', width + 1);
        name[width] = '\0';
        if (strlen(dir.name) < width) {
            strcpy(name, dir.name);
            name[strlen(dir.name)] = ' ';
            printu("%s", name);
        } else
            printu("%s", dir.name);
        if (count % 5 == 4) {
            printu("\n");
        }
    }
    closedir_u(dir_fd);
}

void app_mkdir(const char *path) {
    mkdir_u(path);
}

/*
 * parse the command and its arguments
 */

void parse_cmd(char *cmd) {
    char *token = strtok(cmd, " ");
    while (token != NULL) {
        if (strcmp(token, "ls") == 0) {
            token = strtok(token, " ");
            if (token == NULL) {
                app_ls(current_dir);
            } else {
                do {
                    app_ls(token);
                    token = strtok(token, " ");
                } while (token != NULL);
            }
        }
    }
}

int main() {
    current_dir = naive_malloc();
    read_cwd(current_dir);
    int to_exit = 0;
    char cmd[100];
    while (!to_exit) {
        printu("~%s $ ", current_dir);
        getsu(cmd, 100);
        parse_cmd(cmd);
    }

    naive_free(current_dir);
    exit(0);
    return 0;
}