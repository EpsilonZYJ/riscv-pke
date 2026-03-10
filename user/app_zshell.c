//
// Created by 周煜杰 on 2026/3/10.
//
#include "user_lib.h"
#include "string.h"
#include "util/types.h"

char *current_dir = NULL;

void init_shell() {
    current_dir = naive_malloc();
    read_cwd(current_dir);
}

int main(int argc, char *argv[]) {
    char cmd[50];
    char *option = better_malloc(20);
    while (1) {
        printu("~%s $ ", current_dir);
        scanfu("%s", option);
        if (strcmp(option, "exit") == 0) {
            break;
        } else if (strcmp(option, "pwd") == 0) {
            printu("%s\n", current_dir);
        } else if (strcmp(option, "cd") == 0) {
            scanfu("%s", option);
            if (change_cwd(option) != 0)
                printu("cd: no such file or directory: %s\n", option);
            else
                read_cwd(current_dir);
        } else if (strcmp(option, "cat")) {
            scanfu("%s", option);
            int MAXBUF = 512;
            char buf[MAXBUF];
            int fd = open(option, O_RDWR);
            read_u(fd, buf, MAXBUF);
            printu("%s\n", buf);
            close(fd);
        }
    }
    exit(0);
    return 0;
}
