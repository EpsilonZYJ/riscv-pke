//
// Created by 周煜杰 on 2026/3/11.
//

#ifndef RISCV_PKE_ZSHRC_PARSER_H
#define RISCV_PKE_ZSHRC_PARSER_H

#include "global.h"

#define ZSHRC_PATH "/.zshrc"
#define MAX_ALIAS_NAME 50
#define MAX_ALIAS_VALUE 100
#define ZSHRC_BUF_SIZE 512

typedef struct alias_entry {
    char name[MAX_ALIAS_NAME];
    char value[MAX_ALIAS_VALUE];
    struct alias_entry *next;
} alias_entry_t;

extern alias_entry_t *alias_list;

void add_alias(const char *name, const char *value);
const char *find_alias(const char *name);
void free_aliases();
void load_zshrc();

#endif // RISCV_PKE_ZSHRC_PARSER_H
