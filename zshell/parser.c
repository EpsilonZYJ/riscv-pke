//
// Created by 周煜杰 on 2026/3/11.
//

#include "parser.h"

#include "../user/user_lib.h"
#include "../util/string.h"

char *get_token(char *input, char *delim) {
    if (input == NULL) return NULL;
    int i = 0;
    while (input[i] != '\0' && strchr(delim, input[i]) != NULL) {
        i++;
    }
    if (input[i] == '\0' && input[i + 1] == '\0') return NULL;
    while (input[i] != '\0' && strchr(delim, input[i]) == NULL) {
        i++;
    }
    input[i] = '\0';
    return input;
}

char *skip_current_token(char *input, char *delim) {
    if (input == NULL) return NULL;
    int i = 0;
    int ret_i = 0;
    // 跳过当前token
    while ((input[i] != '\0' && strchr(delim, input[i]) == NULL)) {
        i++;
    }
    // 如果到达字符串结尾，返回NULL
    if (input[i] == '\0' && input[i + 1] == '\0') {
        return NULL;
    } else {
        i++; // 跳过当前token的结尾'\0'
    }

    // 跳过分隔符
    while (input[i] != '\0' && strchr(delim, input[i]) != NULL) {
        i++;
    }
    if (input[i] == '\0' && input[i + 1] == '\0') {
        return NULL;
    } else {
        ret_i = i;
    }
    while (input[i] != '\0' && strchr(delim, input[i]) == NULL) {
        i++;
    }
    input[i] = '\0';
    return input + ret_i;
}

void free_paras(command_t *command) {
    if (command == NULL) return;
    paras_t *paras = command->paras;
    while (paras != NULL) {
        paras_t *next = paras->next;
        better_free(paras);
        paras = next;
    }
    command->para_num = 0;
    command->paras = NULL;
    command->op_type = OP_DEAD;
}

command_t *build_command(char *cmdline, command_t *command) {
    char *token = get_token(cmdline, " ");
    // 如果没有token，说明命令行为空，返回NULL
    if (token == NULL) {
        if (command == NULL) {
            return NULL;
        }
        // 将原来的命令标记为死命令，从而在执行时直接跳过
        for (command_t *i = command; i != NULL; i = i->next) {
            i->op_type = OP_DEAD;
        }
        return command;
    }
    if (command == NULL) {
        command = better_malloc(sizeof(command_t));
        if (command == NULL) {
            return NULL;
        }
        command->paras = NULL;
        command->next = NULL;
    }
    command->para_num = 0;
    command_t *cur_command = command;
    while (token != NULL) {
        cur_command->operation = token;
        token = skip_current_token(token, " ");
        if (token == NULL) {
            // 如果只有一个token，说明是一个没有参数的命令
            cur_command->op_type = OP_EXEC;
            break;
        } else if (strcmp(token, "&") == 0) {
            // 如果下一个token是&，说明是一个多命令
            cur_command->op_type = OP_MULTISTART;
            token = skip_current_token(token, " ");
            if (token == NULL) {
                printu("Error: & should be followed by a command\n");
                free_paras(command);
                return command;
            }
        } else if (strcmp(token, "|") == 0) {
            // 如果下一个token是|，说明是一个管道命令
            cur_command->op_type = OP_PIPLINE;
            token = skip_current_token(token, " ");
            if (token == NULL) {
                printu("Error: | should be followed by a command\n");
                free_paras(command);
                return command;
            }
        } else {
            // 否则，说明是一个普通命令
            paras_t *head = better_malloc(sizeof(paras_t));
            if (head == NULL) {
                free_paras(command);
                return command;
            }
            paras_t *last_para = head;
            int is_pipline = strcmp(token, "|");
            int is_multistart = strcmp(token, "&");
            while (token != NULL && is_multistart != 0 && is_pipline) {
                cur_command->para_num++;
                last_para->next = better_malloc(sizeof(paras_t));
                if (last_para->next == NULL) {
                    // 如果分配内存失败，释放已经分配的内存并返回NULL
                    for (paras_t *i = head; i != NULL; i = i->next) {
                        better_free(i);
                    }
                    free_paras(command);
                    return command;
                }
                last_para = last_para->next;
                last_para->para = token;
                last_para->next = NULL;
                token = skip_current_token(token, " ");
                if (token != NULL) {
                    is_pipline = strcmp(token, "|");
                    is_multistart = strcmp(token, "&");
                } else {
                    is_pipline = 1;
                    is_multistart = 1;
                }
            }
            cur_command->paras = head->next;
            better_free(head);
            // 如果下一个token是&，说明是一个多命令
            if (is_multistart == 0) {
                cur_command->op_type = OP_MULTISTART;
                token = skip_current_token(token, " ");
                if (token == NULL) {
                    printu("Error: & should be followed by a command\n");
                    free_paras(command);
                    return command;
                }
            }
            // 如果下一个token是|，说明是一个管道命令
            else if (is_pipline == 0) {
                cur_command->op_type = OP_PIPLINE;
                token = skip_current_token(token, " ");
                if (token == NULL) {
                    printu("Error: | should be followed by a command\n");
                    free_paras(command);
                    return command;
                }
            } else {
                // 否则，说明是一个普通命令
                cur_command->op_type = OP_EXEC;
                break;
            }
            if (cur_command->next == NULL) {
                // 如果当前命令的下一个命令不存在，说明复用结构体需要增加一个命令
                cur_command->next = better_malloc(sizeof(command_t));
                if (cur_command->next == NULL) {
                    free_paras(command);
                    return command;
                }
                cur_command->next->paras = NULL;
                cur_command->next->next = NULL;
                cur_command->next->operation = NULL;
                cur_command->next->para_num = 0;
                cur_command->next->op_type = OP_DEAD;
            }
            cur_command = cur_command->next;
        }
    }
    return command;
}

void free_command(command_t *command) {
    if (command == NULL) return;
    free_paras(command);
    command_t *cur_command = command;
    while (cur_command != NULL) {
        command_t *next_command = cur_command->next;
        better_free(cur_command);
        cur_command = next_command;
    }
}

void print_command(command_t *command) {
    if (command == NULL) {
        printu("command is NULL\n");
        return;
    }
    command_t *cur_command = command;
    while (cur_command != NULL) {
        printu("operation: %s, para_num: %d, op_type: %d\n", cur_command->operation, cur_command->para_num, cur_command->op_type);
        paras_t *paras = cur_command->paras;
        while (paras != NULL) {
            printu("\tpara: %s\n", paras->para);
            paras = paras->next;
        }
        cur_command = cur_command->next;
    }
}

// 将命令标记为死命令，从而在执行时直接跳过
void clear_command(command_t *command) {
    if (command == NULL) return;
    free_paras(command);
    printu("clear command\n");
    command_t *cur_command = command;
    while (cur_command != NULL) {
        cur_command->operation = NULL;
        cur_command->next = NULL;
        cur_command->para_num = 0;
        cur_command->op_type = OP_DEAD;
        cur_command = cur_command->next;
    }
}