#include "user/user_lib.h"

#ifndef PINGPONG_ITERS
#define PINGPONG_ITERS 5000
#endif

int main(void) {
    int sem_parent = sem_new(1);
    int sem_child = sem_new(0);
    if (sem_parent < 0 || sem_child < 0) {
        printu("pingpong sem init failed\n");
        exit(-1);
        return -1;
    }

    int pid = fork();
    if (pid < 0) {
        printu("pingpong fork failed\n");
        exit(-1);
        return -1;
    }

    if (pid == 0) {
        for (int i = 0; i < PINGPONG_ITERS; i++) {
            sem_P(sem_child);
            sem_V(sem_parent);
        }
        exit(0);
        return 0;
    }

    for (int i = 0; i < PINGPONG_ITERS; i++) {
        sem_P(sem_parent);
        sem_V(sem_child);
    }

    wait(pid);
    printu("pingpong iter=%d done\n", PINGPONG_ITERS);

    exit(0);
    return 0;
}
