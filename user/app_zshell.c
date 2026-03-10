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
    char *registered_apps[] = {
        "app_print_backtrace",
        "app_errorline",
        "app_sum_sequence",
        "app_singlepageheap",
        "app_wait",
        "app_semaphore",
        "app_cow",
        "app_relativepath",
        "app_exec",
        "app_shell",
    };

    char *registered_app_paths[] = {
        "/bin/app_print_backtrace",
        "/bin/app_errorline",
        "/bin/app_sum_sequence",
        "/bin/app_singlepageheap",
        "/bin/app_wait",
        "/bin/app_semaphore",
        "/bin/app_cow",
        "/bin/app_relativepath",
        "/bin/app_exec",
        "/bin/app_shell",
    };
    char cmd[50];
    char *option = better_malloc(100);
    init_shell();
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
        } else if (strcmp(option, "cat") == 0) {
            scanfu("%s", option);
            int MAXBUF = 512;
            char buf[MAXBUF];
            int fd = open(option, O_RDWR);
            read_u(fd, buf, MAXBUF);
            printu("%s\n", buf);
            close(fd);
        } else {
            for (int i = 0; i < sizeof(registered_apps) / sizeof(char *); i++) {
                if (strcmp(option, registered_apps[i]) == 0) {
                    int pid = fork();
                    if (pid == 0) {
                        int ret = exec(registered_app_paths[i], "");
                        if (ret == -1)
                            printu("exec failed!\n");
                    } else {
                        wait(pid);
                    }
                }
            }
        }
    }
    better_free(option);
    exit(0);
    return 0;
}
//
// int main(int argc, char *argv[]) {
//     printu("\n======== Shell Start ========\n\n");
//     char *a = better_malloc(20);
//     scanfu("%s", a);
//     printu("%s\n", a);
//     better_free(a);
//     exit(0);
//     return 0;
// }