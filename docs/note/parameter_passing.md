# 操作系统参数传递给用户程序Main函数的机制说明

## 1. 概述

在本代理操作系统内核（PKE）中，参数传递给用户程序的main函数主要通过以下步骤完成：

1. 启动时解析命令行参数
2. 将参数存储在特定的数据结构中
3. 在程序加载过程中设置寄存器值
4. 用户程序启动时通过main函数接收参数

## 2. 参数传递的详细流程

### 2.1 内核启动与参数解析

在`kernel/kernel.c`文件中，内核启动函数`s_start()`负责初始化系统并加载用户程序：

```c
process* load_user_program() {
  process* proc;
  proc = alloc_process();
  sprint("User application is loading.\n");
  arg_buf arg_bug_msg;
  
  // retrieve command line arguements
  size_t argc = parse_args(&arg_bug_msg);
  if (!argc) panic("You need to specify the application program!\n");
  
  load_bincode_from_host_elf(proc, arg_bug_msg.argv[0]);
  return proc;
}
```

其中`parse_args()`函数负责解析命令行参数：

```c
static size_t parse_args(arg_buf *arg_bug_msg) {
  // HTIFSYS_getmainvars frontend call reads command arguments to (input) *arg_bug_msg
  long r = frontend_syscall(HTIFSYS_getmainvars, (uint64)arg_bug_msg,
      sizeof(*arg_bug_msg), 0, 0, 0, 0, 0);
  kassert(r == 0);

  size_t pk_argc = arg_bug_msg->buf[0];
  uint64 *pk_argv = &arg_bug_msg->buf[1];

  int arg = 1;  // skip the PKE OS kernel string, leave behind only the application name
  for (size_t i = 0; arg + i < pk_argc; i++)
    arg_bug_msg->argv[i] = (char *)(uintptr_t)pk_argv[arg + i];

  //returns the number of strings after PKE kernel in command line
  return pk_argc - arg;
}
```

### 2.2 程序加载与入口设置

在`kernel/elf.c`中，`load_bincode_from_host_elf()`函数负责加载ELF格式的用户程序：

```c
void load_bincode_from_host_elf(process *p, char *filename) {
  // ... 其他代码 ...
  
  // entry (virtual, also physical in lab1_x) address
  p->trapframe->epc = elfloader.ehdr.entry;
  
  // ... 其他代码 ...
}
```

这里设置了程序的入口点地址（epc寄存器），该地址指向用户程序的main函数。

### 2.3 进程切换与参数传递

在`kernel/process.c`中，`switch_to()`函数负责切换到用户模式：

```c
void switch_to(process *proc) {
  assert(proc);
  current = proc;

  // 设置各种寄存器
  write_csr(stvec, (uint64)smode_trap_vector);
  
  // 设置trapframe值
  proc->trapframe->kernel_sp = proc->kstack;
  proc->trapframe->kernel_satp = read_csr(satp);
  proc->trapframe->kernel_trap = (uint64)smode_trap_handler;

  // 设置S模式状态寄存器
  unsigned long x = read_csr(sstatus);
  x &= ~SSTATUS_SPP; // 设置为用户模式
  x |= SSTATUS_SPIE; // 启用中断
  
  write_csr(sstatus, x);

  // 设置程序计数器为ELF入口地址
  write_csr(sepc, proc->trapframe->epc);

  // 切换到用户模式
  uint64 user_satp = MAKE_SATP(proc->pagetable);
  return_to_user(proc->trapframe, user_satp);
}
```

### 2.4 Trapframe结构与寄存器映射

在`kernel/riscv.h`中定义了RISC-V寄存器结构：

```c
typedef struct riscv_regs_t {
  /*  0  */ uint64 ra;
  /*  8  */ uint64 sp;
  /*  16 */ uint64 gp;
  /*  24 */ uint64 tp;
  /*  32 */ uint64 t0;
  /*  40 */ uint64 t1;
  /*  48 */ uint64 t2;
  /*  56 */ uint64 s0;
  /*  64 */ uint64 s1;
  /*  72 */ uint64 a0;  // argc参数
  /*  80 */ uint64 a1;  // argv参数
  /*  88 */ uint64 a2;
  /*  96 */ uint64 a3;
  /* 104 */ uint64 a4;
  /* 112 */ uint64 a5;
  /* 120 */ uint64 a6;
  /* 128 */ uint64 a7;
  // ... 其他寄存器
} riscv_regs;
```

### 2.5 用户程序启动

当用户程序启动时，main函数的签名如下：

```c
int main(int argc, char *argv[])
```

在RISC-V架构中，参数通过寄存器传递：
- `a0`寄存器包含argc（参数数量）
- `a1`寄存器包含argv（参数数组指针）

## 3. 实际示例

以`user/app_cat.c`为例：

```c
int main(int argc, char *argv[]) {
  int fd;
  int MAXBUF = 512;
  char buf[MAXBUF];
  char *filename = argv[0];  // 获取第一个参数作为文件名

  printu("\n======== cat command ========\n");
  printu("cat: %s\n", filename);

  fd = open(filename, O_RDWR);
  printu("file descriptor fd: %d\n", fd);

  read_u(fd, buf, MAXBUF);
  printu("read content: \n%s\n", buf);
  close(fd);

  exit(0);
  return 0;
}
```

## 4. 参数传递流程总结

1. 系统启动时通过`parse_args()`解析命令行参数
2. 参数存储在`arg_buf`结构中
3. 通过`load_bincode_from_host_elf()`加载用户程序并设置入口点
4. 在`switch_to()`函数中设置相关寄存器并切换到用户模式
5. 用户程序启动时，`a0`和`a1`寄存器分别包含argc和argv
6. 用户程序的main函数可以直接访问这些参数

这种设计遵循了标准的RISC-V ABI（应用程序二进制接口），确保了参数能够正确地从内核传递到用户程序。