#include "user/user_lib.h"

#ifndef TARGET_FORKS
#define TARGET_FORKS 10
#endif

int main(void) {
    int created = 0;

    for (int i = 0; i < TARGET_FORKS; i++) {
        int pid = fork();
        if (pid < 0)
            break;
        if (pid == 0) {
            exit(0);
            return 0;
        }
        created++;
    }

    for (int i = 0; i < created; i++)
        wait(-1);

    printu("fork_bench target=%d created=%d\n", TARGET_FORKS, created);
    if (created != TARGET_FORKS) {
        exit(-1);
        return -1;
    }

    exit(0);
    return 0;
}
