# 利用创建索引的方式来构建目录索引

```makefile
# we assume that the utilities from RISC-V cross-compiler (i.e., riscv64-unknown-elf-gcc and etc.)
# are in your system PATH. To check if your environment satisfies this requirement, simple use 
# `which` command as follows:
# $ which riscv64-unknown-elf-gcc
# if you have an output path, your environment satisfy our requirement.

# ---------------------	macros --------------------------
CROSS_PREFIX 	:= riscv64-unknown-elf-
CC 				:= $(CROSS_PREFIX)gcc
AR 				:= $(CROSS_PREFIX)ar
RANLIB        	:= $(CROSS_PREFIX)ranlib

SRC_DIR        	:= .
OBJ_DIR 		:= obj
SPROJS_INCLUDE 	:= -I.  

HOSTFS_ROOT := hostfs_root
HOSTFS_INDEX := $(HOSTFS_ROOT)/.hostfs_index
ifneq (,)
  march := -march=
  is_32bit := $(findstring 32,$(march))
  mabi := -mabi=$(if $(is_32bit),ilp32,lp64)
endif

CFLAGS        	:= -Wall -Werror -gdwarf-3 -fno-builtin -nostdlib -D__NO_INLINE__ -mcmodel=medany -g -Og -std=gnu99 -Wno-unused -Wno-attributes -fno-delete-null-pointer-checks -fno-PIE $(march) -fno-omit-frame-pointer
COMPILE       	:= $(CC) -MMD -MP $(CFLAGS) $(SPROJS_INCLUDE)

#---------------------	utils -----------------------
UTIL_CPPS 	:= util/*.c

UTIL_CPPS  := $(wildcard $(UTIL_CPPS))
UTIL_OBJS  :=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(UTIL_CPPS)))


UTIL_LIB   := $(OBJ_DIR)/util.a

#---------------------	kernel -----------------------
KERNEL_LDS  	:= kernel/kernel.lds
KERNEL_CPPS 	:= \
	kernel/*.c \
	kernel/machine/*.c \
	kernel/util/*.c

KERNEL_ASMS 	:= \
	kernel/*.S \
	kernel/machine/*.S \
	kernel/util/*.S

KERNEL_CPPS  	:= $(wildcard $(KERNEL_CPPS))
KERNEL_ASMS  	:= $(wildcard $(KERNEL_ASMS))
KERNEL_OBJS  	:=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(KERNEL_CPPS)))
KERNEL_OBJS  	+=  $(addprefix $(OBJ_DIR)/, $(patsubst %.S,%.o,$(KERNEL_ASMS)))

KERNEL_TARGET = $(OBJ_DIR)/riscv-pke


#---------------------	spike interface library -----------------------
SPIKE_INF_CPPS 	:= spike_interface/*.c

SPIKE_INF_CPPS  := $(wildcard $(SPIKE_INF_CPPS))
SPIKE_INF_OBJS 	:=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(SPIKE_INF_CPPS)))


SPIKE_INF_LIB   := $(OBJ_DIR)/spike_interface.a


#---------------------	user   -----------------------

USER_MULTI_LDS0  := user/user0.lds
USER_MULTI_LDS1  := user/user1.lds

USER_MULTI_CPP0 		:= user/app0.c user/user_lib.c
USER_MULTI_CPP1 		:= user/app1.c user/user_lib.c

USER_MULTIMEM_CPP0 		:= user/app_alloc0.c user/user_lib.c
USER_MULTIMEM_CPP1 		:= user/app_alloc1.c user/user_lib.c

USER_SHELL_CPPS 	:= user/app_shell.c user/user_lib.c

USER_ZSHELL_CPPS 	:= zshell/zshell.c zshell/command.c zshell/parser.c user/user_lib.c

USER_MZSHELL_CPPS 	:= user/app_zshell.c user/user_lib.c

USER_EXEC_CPPS 		:= user/app_exec.c user/user_lib.c

USER_RELA_CPPS		:= user/app_relativepath.c user/user_lib.c

USER_COW_CPPS		:= user/app_cow.c user/user_lib.c

USER_SEMA_CPPS		:= user/app_semaphore.c user/user_lib.c

USER_WAIT_CPPS		:= user/app_wait.c user/user_lib.c

USER_SUM_CPPS		:= user/app_sum_sequence.c user/user_lib.c

USER_SING_CPPS		:= user/app_singlepageheap.c user/user_lib.c

USER_PRINT_CPPS		:= user/app_print_backtrace.c user/user_lib.c

USER_ERROR_CPPS		:= user/app_errorline.c user/user_lib.c

#--

USER_MULTI_OBJ0		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_MULTI_CPP0)))
USER_MULTI_OBJ1		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_MULTI_CPP1)))

USER_MULTIMEM_OBJ0	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_MULTIMEM_CPP0)))
USER_MULTIMEM_OBJ1	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_MULTIMEM_CPP1)))

USER_SHELL_OBJS  	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_SHELL_CPPS)))

USER_ZSHELL_OBJS  	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_ZSHELL_CPPS)))

USER_MZSHELL_OBJS  	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_MZSHELL_CPPS)))

USER_EXEC_OBJS  	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_EXEC_CPPS)))

USER_RELA_OBJS  	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_RELA_CPPS)))

USER_COW_OBJS		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_COW_CPPS)))

USER_SEMA_OBJS  	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_SEMA_CPPS)))

USER_WAIT_OBJS  	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_WAIT_CPPS)))

USER_SUM_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_SUM_CPPS)))

USER_SING_OBJS  	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_SING_CPPS)))

USER_PRINT_OBJS  	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_PRINT_CPPS)))

USER_ERROR_OBJS  	:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_ERROR_CPPS)))

#--

USER_MULTI_TARGET0 	:= $(HOSTFS_ROOT)/bin/app0
USER_MULTI_TARGET1 	:= $(HOSTFS_ROOT)/bin/app1

USER_MULTIMEM_TARGET0 	:= $(HOSTFS_ROOT)/bin/app_alloc0
USER_MULTIMEM_TARGET1 	:= $(HOSTFS_ROOT)/bin/app_alloc1

USER_SHELL_TARGET 	:= $(HOSTFS_ROOT)/bin/app_shell

USER_ZSHELL_TARGET 	:= $(HOSTFS_ROOT)/bin/zsh

USER_MZSHELL_TARGET 	:= $(HOSTFS_ROOT)/bin/app_zshell

USER_EXEC_TARGET 	:= $(HOSTFS_ROOT)/bin/app_exec

USER_RELA_TARGET	:= $(HOSTFS_ROOT)/bin/app_relativepath

USER_COW_TARGET		:= $(HOSTFS_ROOT)/bin/app_cow

USER_SEMA_TARGET	:= $(HOSTFS_ROOT)/bin/app_semaphore

USER_WAIT_TARGET	:= $(HOSTFS_ROOT)/bin/app_wait

USER_SUM_TARGET		:= $(HOSTFS_ROOT)/bin/app_sum_sequence

USER_SING_TARGET	:= $(HOSTFS_ROOT)/bin/app_singlepageheap

USER_PRINT_TARGET	:= $(HOSTFS_ROOT)/bin/app_print_backtrace

USER_ERROR_TARGET	:= $(HOSTFS_ROOT)/bin/app_errorline

#--

USER_E_CPPS 		:= user/app_ls.c user/user_lib.c

USER_E_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_E_CPPS)))

USER_E_TARGET 		:= $(HOSTFS_ROOT)/bin/app_ls

USER_M_CPPS 		:= user/app_mkdir.c user/user_lib.c

USER_M_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_M_CPPS)))

USER_M_TARGET 		:= $(HOSTFS_ROOT)/bin/app_mkdir

USER_T_CPPS 		:= user/app_touch.c user/user_lib.c

USER_T_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_T_CPPS)))

USER_T_TARGET 		:= $(HOSTFS_ROOT)/bin/app_touch

USER_C_CPPS 		:= user/app_cat.c user/user_lib.c

USER_C_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_C_CPPS)))

USER_C_TARGET 		:= $(HOSTFS_ROOT)/bin/app_cat

USER_O_CPPS 		:= user/app_echo.c user/user_lib.c

USER_O_OBJS  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_O_CPPS)))

USER_O_TARGET 		:= $(HOSTFS_ROOT)/bin/app_echo
#------------------------targets------------------------
$(OBJ_DIR):
	@-mkdir -p $(OBJ_DIR)
	@-mkdir -p $(dir $(UTIL_OBJS))
	@-mkdir -p $(dir $(SPIKE_INF_OBJS))
	@-mkdir -p $(dir $(KERNEL_OBJS))
	@-mkdir -p $(dir $(USER_SHELL_OBJS))
	@-mkdir -p $(dir $(USER_ZSHELL_OBJS))
	@-mkdir -p $(dir $(USER_MZSHELL_OBJS))
	@-mkdir -p $(dir $(USER_EXEC_OBJS))
	@-mkdir -p $(dir $(USER_RELA_OBJS))
	@-mkdir -p $(dir $(USER_COW_OBJS))
	@-mkdir -p $(dir $(USER_SEMA_OBJS))
	@-mkdir -p $(dir $(USER_WAIT_OBJS))
	@-mkdir -p $(dir $(USER_SUM_OBJS))
	@-mkdir -p $(dir $(USER_SING_OBJS))
	@-mkdir -p $(dir $(USER_PRINT_OBJS))
	@-mkdir -p $(dir $(USER_ERROR_OBJS))
	@-mkdir -p $(dir $(USER_MULTI_OBJ0))
	@-mkdir -p $(dir $(USER_MULTI_OBJ1))
	@-mkdir -p $(dir $(USER_MULTIMEM_OBJ0))
	@-mkdir -p $(dir $(USER_MULTIMEM_OBJ1))
	@-mkdir -p $(dir $(USER_E_OBJS))
	@-mkdir -p $(dir $(USER_M_OBJS))
	@-mkdir -p $(dir $(USER_T_OBJS))
	@-mkdir -p $(dir $(USER_C_OBJS))
	@-mkdir -p $(dir $(USER_O_OBJS))

$(OBJ_DIR)/%.o : %.c
	@echo "compiling" $<
	@$(COMPILE) -c $< -o $@

$(OBJ_DIR)/%.o : %.S
	@echo "compiling" $<
	@$(COMPILE) -c $< -o $@

$(UTIL_LIB): $(OBJ_DIR) $(UTIL_OBJS)
	@echo "linking " $@	...
	@$(AR) -rcs $@ $(UTIL_OBJS)
	@echo "Util lib has been build into" \"$@\"

$(SPIKE_INF_LIB): $(OBJ_DIR) $(UTIL_OBJS) $(SPIKE_INF_OBJS)
	@echo "linking " $@	...
	@$(AR) -rcs $@ $(SPIKE_INF_OBJS) $(UTIL_OBJS)
	@echo "Spike lib has been build into" \"$@\"

$(KERNEL_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(SPIKE_INF_LIB) $(KERNEL_OBJS) $(KERNEL_LDS)
	@echo "linking" $@ ...
	@$(COMPILE) $(KERNEL_OBJS) $(UTIL_LIB) $(SPIKE_INF_LIB) -o $@ -T $(KERNEL_LDS)
	@echo "PKE core has been built into" \"$@\"

$(USER_SHELL_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_SHELL_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_SHELL_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_ZSHELL_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_ZSHELL_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_ZSHELL_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_MZSHELL_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_MZSHELL_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_MZSHELL_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_EXEC_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_EXEC_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_EXEC_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_RELA_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_RELA_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_RELA_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_COW_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_COW_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_COW_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_SEMA_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_SEMA_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_SEMA_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_WAIT_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_WAIT_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_WAIT_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_SUM_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_SUM_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_SUM_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_SING_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_SING_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_SING_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_PRINT_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_PRINT_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_PRINT_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_ERROR_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_ERROR_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_ERROR_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"
	@cp $@ $(OBJ_DIR)

$(USER_MULTI_TARGET0): $(OBJ_DIR) $(UTIL_LIB) $(USER_MULTI_OBJ0) $(USER_MULTI_LDS0)
	@echo "linking" $@	...
	@$(COMPILE) $(USER_MULTI_OBJ0) $(UTIL_LIB) -o $@ -T $(USER_MULTI_LDS0)
	@echo "User app has been built into" \"$@\"

$(USER_MULTI_TARGET1): $(OBJ_DIR) $(UTIL_LIB) $(USER_MULTI_OBJ1) $(USER_MULTI_LDS1)
	@echo "linking" $@	...
	@$(COMPILE) $(USER_MULTI_OBJ1) $(UTIL_LIB) -o $@ -T $(USER_MULTI_LDS1)
	@echo "User app has been built into" \"$@\"

$(USER_MULTIMEM_TARGET0): $(OBJ_DIR) $(UTIL_LIB) $(USER_MULTIMEM_OBJ0)
	@echo "linking" $@	...
	@$(COMPILE) $(USER_MULTIMEM_OBJ0) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_MULTIMEM_TARGET1): $(OBJ_DIR) $(UTIL_LIB) $(USER_MULTIMEM_OBJ1)
	@echo "linking" $@	...
	@$(COMPILE) $(USER_MULTIMEM_OBJ1) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_E_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_E_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_E_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_M_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_M_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_M_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_T_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_T_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_T_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_C_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_C_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_C_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(USER_O_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(USER_O_OBJS)
	@echo "linking" $@	...
	-@mkdir -p $(HOSTFS_ROOT)/bin
	@$(COMPILE) --entry=main $(USER_O_OBJS) $(UTIL_LIB) -o $@
	@echo "User app has been built into" \"$@\"

$(HOSTFS_INDEX): $(USER_MZSHELL_TARGET) $(USER_ZSHELL_TARGET) $(USER_SHELL_TARGET) $(USER_EXEC_TARGET) $(USER_RELA_TARGET) $(USER_COW_TARGET) $(USER_SEMA_TARGET) $(USER_WAIT_TARGET) $(USER_SUM_TARGET) $(USER_SING_TARGET) $(USER_PRINT_TARGET) $(USER_ERROR_TARGET) $(USER_MULTIMEM_TARGET0) $(USER_MULTIMEM_TARGET1) $(USER_MULTI_TARGET0) $(USER_MULTI_TARGET1) $(USER_E_TARGET) $(USER_M_TARGET) $(USER_T_TARGET) $(USER_C_TARGET) $(USER_O_TARGET)
	@echo "generating" $@ ...
	@cd $(HOSTFS_ROOT) && rm -f .hostfs_index && \
	find . -mindepth 1 ! -name ".hostfs_index" | sed 's#^\./##' | while IFS= read -r path; do \
		name=$${path##*/}; \
		dir=$${path%/*}; \
		if [ "$$dir" = "$$path" ]; then dir="/"; else dir="/$$dir"; fi; \
		printf '%s\t%s\tE\n' "$$dir" "$$name"; \
	done > .hostfs_index

-include $(wildcard $(OBJ_DIR)/*/*.d)
-include $(wildcard $(OBJ_DIR)/*/*/*.d)

.DEFAULT_GOAL := $(all)

all: $(KERNEL_TARGET) $(USER_MZSHELL_TARGET) $(USER_ZSHELL_TARGET) $(USER_SHELL_TARGET) $(USER_EXEC_TARGET) $(USER_RELA_TARGET) $(USER_COW_TARGET) $(USER_SEMA_TARGET) $(USER_WAIT_TARGET) $(USER_SUM_TARGET) $(USER_SING_TARGET) $(USER_PRINT_TARGET) $(USER_ERROR_TARGET) $(USER_MULTIMEM_TARGET0) $(USER_MULTIMEM_TARGET1) $(USER_MULTI_TARGET0) $(USER_MULTI_TARGET1) $(USER_E_TARGET) $(USER_M_TARGET) $(USER_T_TARGET) $(USER_C_TARGET) $(USER_O_TARGET) $(HOSTFS_INDEX)
.PHONY:all

run: $(KERNEL_TARGET) $(USER_MZSHELL_TARGET) $(USER_ZSHELL_TARGET) $(USER_SHELL_TARGET) $(USER_EXEC_TARGET) $(USER_RELA_TARGET) $(USER_COW_TARGET) $(USER_SEMA_TARGET) $(USER_WAIT_TARGET) $(USER_SUM_TARGET) $(USER_SING_TARGET) $(USER_PRINT_TARGET) $(USER_ERROR_TARGET) $(USER_MULTIMEM_TARGET0) $(USER_MULTIMEM_TARGET1) $(USER_MULTI_TARGET0) $(USER_MULTI_TARGET1) $(USER_E_TARGET) $(USER_M_TARGET) $(USER_T_TARGET) $(USER_C_TARGET) $(USER_O_TARGET) $(HOSTFS_INDEX)
	@echo "********************HUST PKE********************"
	@echo "********************APP SHELL*********************"
	spike $(KERNEL_TARGET) /bin/app_shell
	@echo "*********************APP EXEC*********************"
	spike $(KERNEL_TARGET) /bin/app_exec
	@echo "*********************APP EXEC*********************"
	spike $(KERNEL_TARGET) /bin/app_relativepath
	@echo "*********************APP COW**********************"
	spike $(KERNEL_TARGET) /bin/app_cow
	@echo "*********************APP SEMA*********************"
	spike $(KERNEL_TARGET) /bin/app_semaphore
	@echo "*********************APP WAIT*********************"
	spike $(KERNEL_TARGET) /bin/app_wait
	@echo "*********************APP SUM**********************"
	spike $(KERNEL_TARGET) /bin/app_sum_sequence
	@echo "*********************APP SING*********************"
	spike $(KERNEL_TARGET) /bin/app_singlepageheap
	@echo "********************APP PRINT*********************"
	spike $(KERNEL_TARGET) /bin/app_print_backtrace
	@echo "********************APP ERROR*********************"
	spike $(KERNEL_TARGET) /bin/app_errorline
	@echo "********************APP MULTI*********************"
	spike -p2 $(KERNEL_TARGET) /bin/app0 /bin/app1
	@echo "******************APP MULTIMEM********************"
	spike -p2 $(KERNEL_TARGET) /bin/app_alloc0 /bin/app_alloc1

# need openocd!
gdb:$(KERNEL_TARGET) $(USER_SHELL_TARGET)
	spike --rbb-port=9824 -H $(KERNEL_TARGET) $(USER_SHELL_TARGET) &
	@sleep 1
	openocd -f ./.spike.cfg &
	@sleep 1
	riscv64-unknown-elf-gdb -command=./.gdbinit

# clean gdb. need openocd!
gdb_clean:
	@-kill -9 $$(lsof -i:9824 -t)
	@-kill -9 $$(lsof -i:3333 -t)
	@sleep 1

objdump:
	riscv64-unknown-elf-objdump -d $(KERNEL_TARGET) > $(OBJ_DIR)/kernel_dump
	riscv64-unknown-elf-objdump -d $(USER_SHELL_TARGET) > $(OBJ_DIR)/user_dump

cscope:
	find ./ -name "*.c" > cscope.files
	find ./ -name "*.h" >> cscope.files
	find ./ -name "*.S" >> cscope.files
	find ./ -name "*.lds" >> cscope.files
	cscope -bqk

format:
	@python ./format.py ./

clean:
	rm -fr ${OBJ_DIR} ${HOSTFS_ROOT}/bin
```

```C++
/*
 * Interface functions between VFS and host-fs. added @lab4_1.
 */
#include "hostfs.h"

#include "pmm.h"
#include "spike_interface/spike_file.h"
#include "spike_interface/spike_utils.h"
#include "util/hash_table.h"
#include "util/string.h"
#include "util/types.h"
#include "vfs.h"

#define HOSTFS_INDEX_FILE H_ROOT_DIR "/.hostfs_index"
#define HOSTFS_INDEX_MAX_BYTES 32768

static void vfs_path_backtrack(char *path, struct dentry *dentry) {
    if (dentry == NULL || dentry->parent == NULL) {
        return;
    }
    vfs_path_backtrack(path, dentry->parent);
    strcat(path, "/");
    strcat(path, dentry->name);
}

static struct dentry *hostfs_find_dentry_by_vinode(struct vinode *vinode) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        struct hash_node *node = dentry_hash_table.head[i].next;
        while (node) {
            struct dentry *d = (struct dentry *)node->value;
            if (d && d->dentry_inode == vinode) {
                return d;
            }
            node = node->next;
        }
    }
    return NULL;
}

static int hostfs_get_vfs_path(struct vinode *vinode, char *path) {
    struct dentry *target = hostfs_find_dentry_by_vinode(vinode);
    if (target == NULL) {
        return -1;
    }

    if (target->parent == NULL) {
        strcpy(path, "/");
        return 0;
    }

    path[0] = '\0';
    vfs_path_backtrack(path, target);
    return 0;
}

/**** host-fs vinode interface ****/
const struct vinode_ops hostfs_i_ops = {
    .viop_read = hostfs_read,
    .viop_write = hostfs_write,
    .viop_create = hostfs_create,
    .viop_lseek = hostfs_lseek,
    .viop_lookup = hostfs_lookup,

    .viop_hook_open = hostfs_hook_open,
    .viop_hook_close = hostfs_hook_close,
    .viop_write_back_vinode = hostfs_write_back_vinode,

    // not implemented
    .viop_link = hostfs_link,
    .viop_unlink = hostfs_unlink,
    .viop_readdir = hostfs_readdir,
    .viop_mkdir = hostfs_mkdir,
};

/**** hostfs utility functions ****/
//
// append hostfs to the fs list.
//
int register_hostfs() {
    struct file_system_type *fs_type = (struct file_system_type *)alloc_page();
    fs_type->type_num = HOSTFS_TYPE;
    fs_type->get_superblock = hostfs_get_superblock;

    for (int i = 0; i < MAX_SUPPORTED_FS; i++) {
        if (fs_list[i] == NULL) {
            fs_list[i] = fs_type;
            return 0;
        }
    }
    return -1;
}

//
// append new device under "name" to vfs_dev_list.
//
struct device *init_host_device(char *name) {
    // find rfs in registered fs list
    struct file_system_type *fs_type = NULL;
    for (int i = 0; i < MAX_SUPPORTED_FS; i++) {
        if (fs_list[i] != NULL && fs_list[i]->type_num == HOSTFS_TYPE) {
            fs_type = fs_list[i];
            break;
        }
    }
    if (!fs_type)
        panic("init_host_device: No HOSTFS file system found!\n");

    // allocate a vfs device
    struct device *device = (struct device *)alloc_page();
    // set the device name and index
    strcpy(device->dev_name, name);
    // we only support one host-fs device
    device->dev_id = 0;
    device->fs_type = fs_type;

    // add the device to the vfs device list
    for (int i = 0; i < MAX_VFS_DEV; i++) {
        if (vfs_dev_list[i] == NULL) {
            vfs_dev_list[i] = device;
            break;
        }
    }

    return device;
}

//
// recursive call to assemble a path.
//
void path_backtrack(char *path, struct dentry *dentry) {
    if (dentry->parent == NULL) {
        return;
    }
    path_backtrack(path, dentry->parent);
    strcat(path, "/");
    strcat(path, dentry->name);
}

//
// obtain the absolute path for "dentry", from root to file.
//
void get_path_string(char *path, struct dentry *dentry) {
    strcpy(path, H_ROOT_DIR);
    path_backtrack(path, dentry);
}

//
// allocate a vfs inode for an host fs file.
//
struct vinode *hostfs_alloc_vinode(struct super_block *sb) {
    struct vinode *vinode = default_alloc_vinode(sb);
    vinode->inum = -1;
    vinode->i_fs_info = NULL;
    vinode->i_ops = &hostfs_i_ops;
    return vinode;
}

int hostfs_write_back_vinode(struct vinode *vinode) {
    return 0;
}

//
// populate the vfs inode of an hostfs file, according to its stats.
//
int hostfs_update_vinode(struct vinode *vinode) {
    spike_file_t *f = vinode->i_fs_info;
    if ((int64)f < 0) { // is a direntry
        vinode->type = H_DIR;
        return -1;
    }

    struct stat stat;
    spike_file_stat(f, &stat);

    vinode->inum = stat.st_ino;
    vinode->size = stat.st_size;
    vinode->nlinks = stat.st_nlink;
    vinode->blocks = stat.st_blocks;

    if (S_ISDIR(stat.st_mode)) {
        vinode->type = H_DIR;
    } else if (S_ISREG(stat.st_mode)) {
        vinode->type = H_FILE;
    } else {
        sprint("hostfs_lookup:unknown file type!");
        return -1;
    }

    return 0;
}

/**** vfs-host-fs interface functions ****/
//
// read a hostfs file.
//
ssize_t hostfs_read(struct vinode *f_inode, char *r_buf, ssize_t len,
                    int *offset) {
    spike_file_t *pf = (spike_file_t *)f_inode->i_fs_info;
    if (pf < 0) {
        sprint("hostfs_read: invalid file handle!\n");
        return -1;
    }
    int read_len = spike_file_read(pf, r_buf, len);
    // obtain current offset
    *offset = spike_file_lseek(pf, 0, 1);
    return read_len;
}

//
// write a hostfs file.
//
ssize_t hostfs_write(struct vinode *f_inode, const char *w_buf, ssize_t len,
                     int *offset) {
    spike_file_t *pf = (spike_file_t *)f_inode->i_fs_info;
    if (pf < 0) {
        sprint("hostfs_write: invalid file handle!\n");
        return -1;
    }
    int write_len = spike_file_write(pf, w_buf, len);
    // obtain current offset
    *offset = spike_file_lseek(pf, 0, 1);
    return write_len;
}

//
// lookup a hostfs file, and establish its vfs inode in PKE vfs.
//
struct vinode *hostfs_lookup(struct vinode *parent, struct dentry *sub_dentry) {
    // get complete path string
    char path[MAX_PATH_LEN];
    get_path_string(path, sub_dentry);

    spike_file_t *f = spike_file_open(path, O_RDWR, 0);

    struct vinode *child_inode = hostfs_alloc_vinode(parent->sb);
    child_inode->i_fs_info = f;
    hostfs_update_vinode(child_inode);

    child_inode->ref = 0;
    return child_inode;
}

//
// creates a hostfs file, and establish its vfs inode.
//
struct vinode *hostfs_create(struct vinode *parent, struct dentry *sub_dentry) {
    char path[MAX_PATH_LEN];
    get_path_string(path, sub_dentry);

    spike_file_t *f = spike_file_open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if ((int64)f < 0) {
        sprint("hostfs_create cannot create the given file.\n");
        return NULL;
    }

    struct vinode *new_inode = hostfs_alloc_vinode(parent->sb);
    new_inode->i_fs_info = f;

    if (hostfs_update_vinode(new_inode) != 0) return NULL;

    new_inode->ref = 0;
    return new_inode;
}

//
// reposition read/write file offset
//
int hostfs_lseek(struct vinode *f_inode, ssize_t new_offset, int whence,
                 int *offset) {
    spike_file_t *f = (spike_file_t *)f_inode->i_fs_info;
    if (f < 0) {
        sprint("hostfs_lseek: invalid file handle!\n");
        return -1;
    }

    *offset = spike_file_lseek(f, new_offset, whence);
    if (*offset >= 0)
        return 0;
    return -1;
}

int hostfs_link(struct vinode *parent, struct dentry *sub_dentry,
                struct vinode *link_node) {
    panic("hostfs_link not implemented!\n");
    return -1;
}

int hostfs_unlink(struct vinode *parent, struct dentry *sub_dentry, struct vinode *unlink_node) {
    panic("hostfs_unlink not implemented!\n");
    return -1;
}

int hostfs_readdir(struct vinode *dir_vinode, struct dir *dir, int *offset) {
    if (dir_vinode == NULL || dir == NULL || offset == NULL) {
        return -1;
    }

    char dir_path[MAX_PATH_LEN];
    if (hostfs_get_vfs_path(dir_vinode, dir_path) != 0) {
        return -1;
    }

    spike_file_t *idx_file = spike_file_open(HOSTFS_INDEX_FILE, O_RDONLY, 0);
    if ((int64)idx_file < 0) {
        return -1;
    }

    static char index_buf[HOSTFS_INDEX_MAX_BYTES + 1];
    int index_len = spike_file_read(idx_file, index_buf, HOSTFS_INDEX_MAX_BYTES);
    spike_file_close(idx_file);
    if (index_len <= 0) {
        return -1;
    }
    index_buf[index_len] = '\0';

    int matched = 0;
    char *line = index_buf;
    while (*line != '\0') {
        char *next = strchr(line, '\n');
        if (next != NULL) {
            *next = '\0';
        }

        char *sep1 = strchr(line, '\t');
        if (sep1 != NULL) {
            *sep1 = '\0';
            char *name = sep1 + 1;
            char *sep2 = strchr(name, '\t');
            if (sep2 != NULL) {
                *sep2 = '\0';
                if (strcmp(line, dir_path) == 0) {
                    if (matched == *offset) {
                        safestrcpy(dir->name, name, MAX_FILE_NAME_LEN);
                        dir->inum = matched;
                        (*offset)++;
                        return 0;
                    }
                    matched++;
                }
            }
        }

        if (next == NULL) {
            break;
        }
        line = next + 1;
    }

    return -1;
}

struct vinode *hostfs_mkdir(struct vinode *parent, struct dentry *sub_dentry) {
    panic("hostfs_mkdir not implemented!\n");
    return NULL;
}

/**** vfs-hostfs hook interface functions ****/
//
// open a hostfs file (after having its vfs inode).
//
int hostfs_hook_open(struct vinode *f_inode, struct dentry *f_dentry) {
    if (f_inode->i_fs_info != NULL) return 0;

    char path[MAX_PATH_LEN];
    get_path_string(path, f_dentry);
    spike_file_t *f = spike_file_open(path, O_RDWR, 0);
    if ((int64)f < 0) {
        sprint("hostfs_hook_open cannot open the given file.\n");
        return -1;
    }

    f_inode->i_fs_info = f;
    return 0;
}

//
// close a hostfs file.
//
int hostfs_hook_close(struct vinode *f_inode, struct dentry *dentry) {
    spike_file_t *f = (spike_file_t *)f_inode->i_fs_info;
    spike_file_close(f);
    return 0;
}

/**** vfs-hostfs file system type interface functions ****/
struct super_block *hostfs_get_superblock(struct device *dev) {
    // set the data for the vfs super block
    struct super_block *sb = alloc_page();
    sb->s_dev = dev;

    struct vinode *root_inode = hostfs_alloc_vinode(sb);
    root_inode->type = H_DIR;

    struct dentry *root_dentry = alloc_vfs_dentry("/", root_inode, NULL);
    sb->s_root = root_dentry;

    return sb;
}

```