//
// Created by 周煜杰 on 2026/3/11.
//

#include "execute.h"
#include "../user/user_lib.h"
#include "../util/string.h"
#include "command.h"
#include "zshrc_parser.h"

int launch_process(char *path, char *para, int wait_child, int *wait_count) {
    int pid = fork();
    if (pid < 0) {
        printu("fork failed!\n");
        return -1;
    }
    if (pid == 0) {
        int ret = exec(path, para);
        if (ret == -1) {
            printu("exec failed!\n");
            exit(-1);
        }
        exit(0);
    }
    if (wait_child == 1) {
        wait(pid);
    } else if (wait_child == -2) {
        wait_count++;
    }
    return 0;
}

int launch_process_to_hart(char *path, char *para, int target_hartid, int wait_child,
                           int *wait_count) {
    int pid = fork_to(target_hartid);
    if (pid < 0) {
        printu("fork_to failed!\n");
        return -1;
    }
    if (pid == 0) {
        int ret = exec(path, para);
        if (ret == -1) {
            printu("exec failed!\n");
            exit(-1);
        }
        exit(0);
    }
    if (wait_child == 1) {
        wait(pid);
    } else if (wait_child == -2) {
        wait_count++;
    }
    return 0;
}

int run_one_command(command_t *cur_command, int wait_child, int *wait_count) {
    if (strcmp(cur_command->operation, "exec") == 0) {
        if (cur_command->para_num == 2) {
            char *path = cur_command->paras->para;
            char *para = cur_command->paras->next->para;
            return launch_process(path, para, wait_child, wait_count);
        } else if (cur_command->para_num == 1) {
            char *path = cur_command->paras->para;
            return launch_process(path, "", wait_child, wait_count);
        } else {
            printu("exec: invalid input!\n");
            return -1;
        }
        return 0;
    }

    if (startwith(cur_command->operation, "./") || startwith(cur_command->operation, "/")) {
        return launch_process(cur_command->operation,
                              cur_command->paras == NULL ? "" : cur_command->paras->para,
                              wait_child, wait_count);
    }

    int ret = exec_supported_command_t(cur_command);
    if (ret == 0) {
        return 0;
    }

    const char *alias_val = find_alias(cur_command->operation);
    if (alias_val != NULL) {
        return launch_process((char *)alias_val,
                              cur_command->paras == NULL ? "" : cur_command->paras->para,
                              wait_child, wait_count);
    } else {
        printu("Error: unknown command: %s\n", cur_command->operation);
        return -1;
    }
    return 0;
}

int run_one_command_multicore(command_t *cur_command, int target_hartid, int wait_child,
                              int *wait_count) {
    if (strcmp(cur_command->operation, "exec") == 0) {
        if (cur_command->para_num == 2) {
            char *path = cur_command->paras->para;
            char *para = cur_command->paras->next->para;
            return launch_process_to_hart(path, para, target_hartid, wait_child, wait_count);
        } else if (cur_command->para_num == 1) {
            char *path = cur_command->paras->para;
            return launch_process_to_hart(path, "", target_hartid, wait_child, wait_count);
        } else {
            printu("exec: invalid input!\n");
            return -1;
        }
        return 0;
    }

    if (startwith(cur_command->operation, "./") || startwith(cur_command->operation, "/")) {
        return launch_process_to_hart(cur_command->operation,
                                      cur_command->paras == NULL ? "" : cur_command->paras->para,
                                      target_hartid, wait_child, wait_count);
    }

    int ret = exec_supported_command_t(cur_command);
    if (ret == 0) {
        return 0;
    }

    const char *alias_val = find_alias(cur_command->operation);
    if (alias_val != NULL) {
        return launch_process_to_hart((char *)alias_val,
                                      cur_command->paras == NULL ? "" : cur_command->paras->para,
                                      target_hartid, wait_child, wait_count);
    } else {
        printu("Error: unknown command: %s\n", cur_command->operation);
        return -1;
    }
    return 0;
}

int exec_supported_command_t(command_t *cur_command) {
    if (strcmp(cur_command->operation, "ls") == 0) {
        if (cur_command->para_num == 0) {
            app_ls(current_dir);
        } else if (cur_command->para_num == 1) {
            app_ls(cur_command->paras->para);
        } else {
            paras_t *para = cur_command->paras;
            while (para != NULL) {
                printu("%s:\n", para->para);
                app_ls(para->para);
                para = para->next;
            }
        }
        return 0;
    } else if (strcmp(cur_command->operation, "cd") == 0) {
        if (cur_command->para_num == 0) {
            app_cd("/");
        } else if (cur_command->para_num == 1) {
            app_cd(cur_command->paras->para);
        } else {
            printu("cd: too many arguments\n");
        }
        return 0;
    } else if (strcmp(cur_command->operation, "cat") == 0) {
        if (cur_command->para_num == 0) {
            printu("cat: missing file operand\n");
        } else if (cur_command->para_num == 1) {
            app_cat(cur_command->paras->para);
        } else {
            paras_t *para = cur_command->paras;
            while (para != NULL) {
                printu("%s:\n", para->para);
                app_cat(para->para);
                para = para->next;
            }
        }
        return 0;
    } else if (strcmp(cur_command->operation, "echo") == 0) {
        if (cur_command->para_num != 2) {
            printu("echo: invalid input!\n");
        } else {
            char *filepath = cur_command->paras->para;
            char *content = cur_command->paras->next->para;
            app_echo(filepath, content);
        }
        return 0;
    } else if (strcmp(cur_command->operation, "cd") == 0) {
        if (cur_command->para_num == 0) {
            app_cd("/");
        } else if (cur_command->para_num == 1) {
            app_cd(cur_command->paras->para);
        } else {
            printu("cd: too many arguments\n");
        }
        return 0;
    } else if (strcmp(cur_command->operation, "pwd") == 0) {
        if (cur_command->para_num == 0) {
            app_pwd();
        } else {
            printu("pwd: too many arguments\n");
        }
        return 0;
    } else if (strcmp(cur_command->operation, "mkdir") == 0) {
        if (cur_command->para_num == 0) {
            printu("mkdir: missing operand\n");
        } else {
            paras_t *para = cur_command->paras;
            while (para != NULL) {
                app_mkdir(para->para);
                para = para->next;
            }
        }
        return 0;
    } else if (strcmp(cur_command->operation, "touch") == 0) {
        if (cur_command->para_num == 0) {
            printu("touch: missing operand\n");
        } else {
            paras_t *para = cur_command->paras;
            while (para != NULL) {
                app_touch(para->para);
                para = para->next;
            }
        }
        return 0;
    }
    return -1;
}

int exec_command(command_t *command) {
    if (command == NULL) return 0;
    command_t *cur_command = command;
    int wait_count = 0;
    int mc_cmd_idx = 0;
    while (cur_command != NULL && cur_command->op_type != OP_DEAD) {
        if (strcmp(cur_command->operation, "exit") == 0) {
            return 1;
        }
        if (cur_command->op_type == OP_EXEC) {
            if (mc_cmd_idx > 0) {
                int ncpu = get_ncpu();
                if (ncpu <= 0) ncpu = 1;
                int target_hartid = mc_cmd_idx % ncpu;
                int ret = run_one_command_multicore(cur_command, target_hartid, -2, &wait_count);
                if (ret == -1) {
                    printu("Error: failed to execute command: %s\n", cur_command->operation);
                    return 0;
                }
                wait_count++;
                mc_cmd_idx++;
            } else {
                run_one_command(cur_command, 1, &wait_count);
            }
            break; // OP_EXEC只会出现一个命令
        } else if (cur_command->op_type == OP_MULTISTART) {
            int ret = run_one_command(cur_command, -2, &wait_count);
            if (ret == -1) {
                printu("Error: failed to execute command: %s\n", cur_command->operation);
                return 0;
            }
            wait_count++;
        } else if (cur_command->op_type == OP_MULTICORE) {
            int ncpu = get_ncpu();
            if (ncpu <= 0) ncpu = 1;
            int target_hartid = mc_cmd_idx % ncpu;
            int ret = run_one_command_multicore(cur_command, target_hartid, -2, &wait_count);
            if (ret == -1) {
                printu("Error: failed to execute command: %s\n", cur_command->operation);
                return 0;
            }
            wait_count++;
            mc_cmd_idx++;
        } else if (cur_command->op_type == OP_PIPLINE) {
            while (cur_command->op_type == OP_PIPLINE) {
                pipline_write();
                int ret = run_one_command(cur_command, 1, &wait_count);
                if (ret != 0) {
                    pipline_reset();
                    printu("Error: failed to execute command: %s\n", cur_command->operation);
                    return 0;
                }
                pipline_read();
                cur_command = cur_command->next;
            }
            int ret = run_one_command(cur_command, 1, &wait_count);
            pipline_reset();
            if (ret != 0) {
                printu("Error: failed to execute command: %s\n", cur_command->operation);
                return 0;
            }
        }
        cur_command = cur_command->next;
    }
    wait_count++;
    while (wait_count > 0) {
        wait(-1);
        wait_count--;
    }
    return 0;
}