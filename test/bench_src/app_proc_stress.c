#include "user/user_lib.h"

#ifndef TARGET_PROCS
#define TARGET_PROCS 20
#endif

int main(void) {
    int created = 0;

    for (int i = 0; i < TARGET_PROCS; i++) {
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

    printu("target=%d created=%d\n", TARGET_PROCS, created);
    exit(0);
    return 0;
}
