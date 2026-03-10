//
// Created by 周煜杰 on 2026/3/10.
//

#include "user_lib.h"

int main() {
    char *s = better_malloc(200);
    printu("start to scan a string:\n");
    scanfu("%s", s);
    printu("Hello, %s!\n", s);
    better_free(s);
    exit(0);
    return 0;
}