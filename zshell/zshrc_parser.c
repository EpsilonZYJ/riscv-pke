//
// Created by 周煜杰 on 2026/3/11.
//

#include "zshrc_parser.h"
#include "../util/string.h"
#include "../user/user_lib.h"
#include "../util/types.h"

alias_entry_t *alias_list = NULL;

void add_alias(const char *name, const char *value) {
    alias_entry_t *a = better_malloc(sizeof(alias_entry_t));
    if (a == NULL) return;
    safestrcpy(a->name, name, MAX_ALIAS_NAME);
    safestrcpy(a->value, value, MAX_ALIAS_VALUE);
    a->next = alias_list;
    alias_list = a;
}

const char *find_alias(const char *name) {
    alias_entry_t *a = alias_list;
    while (a != NULL) {
        if (strcmp(a->name, name) == 0) return a->value;
        a = a->next;
    }
    return NULL;
}

void free_aliases() {
    alias_entry_t *a = alias_list;
    while (a != NULL) {
        alias_entry_t *next = a->next;
        better_free(a);
        a = next;
    }
    alias_list = NULL;
}

/*
 * load_zshrc – open ZSHRC_PATH and parse lines of the form:
 *   alias NAME='VALUE'
 * Each valid alias is inserted into the global alias_list.
 */
void load_zshrc() {
    int fd = open(ZSHRC_PATH, O_RDWR);
    if (fd < 0) return; /* .zshrc absent – silently skip */

    char buf[ZSHRC_BUF_SIZE];
    memset(buf, 0, ZSHRC_BUF_SIZE);
    int n = read_u(fd, buf, ZSHRC_BUF_SIZE - 1);
    close(fd);
    if (n <= 0) return;

    char *p = buf;
    while (*p != '\0') {
        /* find end of current line */
        char *eol = p;
        while (*eol != '\0' && *eol != '\n') eol++;
        char saved = *eol;
        *eol = '\0';

        /* parse "alias NAME='VALUE'" */
        if (startwith(p, "alias ")) {
            char *name_start = p + 6;
            /* find '=' that separates name from value */
            char *eq = name_start;
            while (*eq != '\0' && *eq != '=') eq++;
            if (*eq == '=') {
                *eq = '\0'; /* null-terminate the name in-place */
                char *val_start = eq + 1;
                if (*val_start == '\'') {
                    val_start++;
                    char *val_end = val_start;
                    while (*val_end != '\0' && *val_end != '\'') val_end++;
                    if (*val_end == '\'') {
                        *val_end = '\0'; /* null-terminate the value */
                        add_alias(name_start, val_start);
                    }
                }
            }
        }

        if (saved == '\n')
            p = eol + 1;
        else
            break;
    }
}
