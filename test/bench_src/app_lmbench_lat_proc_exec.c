#include "user/user_lib.h"

#ifndef LATPROC_ITERS
#define LATPROC_ITERS 20
#endif

int main(void) {
    int ok = 0;

    for (int i = 0; i < LATPROC_ITERS; i++) {
        int pid = fork();
        if (pid < 0)
            break;

        if (pid == 0) {
            int ret = exec("/bin/app_lmbench_exec_target", "");
            if (ret < 0)
                exit(-1);
            return 0;
        }

        if (wait(pid) < 0)
            break;
        ok++;
    }

    printu("lmbench_lat_proc_exec iters=%d ok=%d\n", LATPROC_ITERS, ok);

    if (ok != LATPROC_ITERS) {
        exit(-1);
        return -1;
    }

    exit(0);
    return 0;
}
