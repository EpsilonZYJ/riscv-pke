# Spike 运行程序过程详解

本文档详细描述了使用 `spike -p2 ./obj/riscv-pke ./obj/app0 ./obj/app1` 命令运行程序时，系统如何读取命令并执行的完整过程。

## 1. 启动命令分析

```bash
spike -p2 ./obj/riscv-pke ./obj/app0 ./obj/app1
```

- `-p2`: 指定模拟两个硬件线程（hart）
- `./obj/riscv-pke`: PKE 内核镜像文件
- `./obj/app0` 和 `./obj/app1`: 用户应用程序

## 2. Spike 多核启动机制

当使用 `-p2` 参数启动 Spike 时，Spike 会创建两个硬件线程（hart0 和 hart1）。每个 hart 都会独立执行，但需要注意的是：

1. **内核实例**: 虽然有两个 hart，但在内存中只运行一个 PKE 内核实例。两个 hart 共享同一个内核代码和数据。
2. **启动过程**: 每个 hart 都会从 `_mentry` 入口点开始执行，但会根据 hart ID 进行不同的初始化。
3. **参数传递**: Spike 通过寄存器 a0 和 a1 向每个 hart 传递参数：
    - `a0` (x10): hart ID（处理器 ID）
    - `a1` (x11): 指向设备树字符串（DTS）的指针，存储在 Spike 模拟的 RISC-V 计算机内存中

参数传递的具体机制：

- 在汇编代码 `_mentry` 入口中，这两个参数直接传递给 C 函数 `m_start(hartid, dtb)`
- `m_start` 函数在 `kernel/machine/minit.c` 中定义，接收这两个参数并进行相应处理

## 3. Spike 模拟器启动流程

### 3.1 初始化阶段

1. Spike 模拟器启动，创建两个硬件线程（hart0 和 hart1）
2. 加载 `riscv-pke` 内核镜像到内存中
3. 解析设备树（Device Tree Blob, DTB），获取系统配置信息
4. 初始化 HTIF（Host-Target Interface）接口，用于主机和目标系统之间的通信

### 3.2 机器模式（M-Mode）启动

入口点：`kernel/machine/minit.c` 中的 `m_start()` 函数

```c
void m_start(uintptr_t hartid, uintptr_t dtb) {
  // 初始化 Spike 文件接口（stdin, stdout, stderr）
  spike_file_init();
  
  // 初始化 DTB，获取 HTIF 和内存信息
  init_dtb(dtb);
  
  // 设置中断委托，将大部分中断和异常委托给 S-Mode
  delegate_traps();
  
  // 设置定时器中断
  timerinit(hartid);
  
  // 切换到监督模式（S-Mode）
  asm volatile("mret");
}
```

关键步骤：

1. `spike_file_init()`: 初始化标准输入输出文件描述符
2. `init_dtb()`: 解析设备树，获取内存大小和 HTIF 信息
3. `delegate_traps()`: 委托中断和异常处理到 S-Mode
4. `mret`: 切换到 S-Mode，跳转到 `s_start()` 函数

## 4. 监督模式（S-Mode）启动

入口点：`kernel/kernel.c` 中的 `s_start()` 函数

```c
int s_start(void) {
  // 设置地址转换寄存器（Bare 模式，虚拟地址等于物理地址）
  write_csr(satp, 0);
  
  // 加载用户程序
  load_user_program(&user_app);
  
  // 切换到用户模式执行
  switch_to(&user_app);
  
  return 0;
}
```

## 5. 命令行参数处理

在 `kernel/elf.c` 中的 `load_bincode_from_host_elf()` 函数负责处理命令行参数：

```c
static size_t parse_args(arg_buf *arg_bug_msg) {
    // 使用 HTIF 系统调用获取主变量（命令行参数）
    long r = frontend_syscall(HTIFSYS_getmainvars, (uint64)arg_bug_msg,
                              sizeof(*arg_bug_msg), 0, 0, 0, 0, 0);
    
    size_t pk_argc = arg_bug_msg->buf[0];
    uint64 *pk_argv = &arg_bug_msg->buf[1];

    // 跳过内核名称，只保留应用程序名称
    int arg = 1;
    for (size_t i = 0; arg + i < pk_argc; i++)
        arg_bug_msg->argv[i] = (char *)(uintptr_t)pk_argv[arg + i];

    return pk_argc - arg;  // 返回应用程序数量
}
```

处理流程：

1. 通过 `frontend_syscall(HTIFSYS_getmainvars, ...)` 获取完整的命令行参数
2. 参数格式为：`[argc][argv0][argv1][argv2]...`
3. 第一个参数是内核名称（`./obj/riscv-pke`），跳过它
4. 剩余参数是用户应用程序（`./obj/app0` 和 `./obj/app1`）

## 6. 用户程序加载

`load_bincode_from_host_elf()` 函数继续加载第一个用户程序：

```c
void load_bincode_from_host_elf(process *p) {
    // 解析命令行参数
    size_t argc = parse_args(&arg_bug_msg);
    if (!argc) panic("You need to specify the application program!\n");

    sprint("Application: %s\n", arg_bug_msg.argv[0]);

    // 打开 ELF 文件
    info.f = spike_file_open(arg_bug_msg.argv[0], O_RDONLY, 0);
    
    // 初始化 ELF 加载器
    if (elf_init(&elfloader, &info) != EL_OK)
        panic("fail to init elfloader.\n");

    // 加载 ELF 段到内存
    if (elf_load(&elfloader) != EL_OK) panic("Fail on loading elf.\n");

    // 设置程序入口点
    p->trapframe->epc = elfloader.ehdr.entry;
    
    // 关闭文件
    spike_file_close(info.f);
}
```

加载过程：

1. 使用 `spike_file_open()` 打开第一个应用程序文件（`./obj/app0`）
2. 初始化 ELF 加载器上下文
3. 读取 ELF 头部信息，验证魔数
4. 遍历程序段头，将可加载段复制到内存中
5. 设置程序计数器（epc）为 ELF 入口地址

## 7. 进程切换到用户模式

`switch_to()` 函数在 `kernel/process.c` 中实现：

```c
void switch_to(process* proc) {
    current = proc;

    // 设置 S-Mode 中断向量
    write_csr(stvec, (uint64)smode_trap_vector);

    // 设置陷阱帧值
    proc->trapframe->kernel_sp = proc->kstack;
    proc->trapframe->kernel_trap = (uint64)smode_trap_handler;

    // 设置状态寄存器，切换到用户模式
    unsigned long x = read_csr(sstatus);
    x &= ~SSTATUS_SPP;  // 清除 SPP，设置为用户模式
    x |= SSTATUS_SPIE;  // 启用用户模式中断
    write_csr(sstatus, x);

    // 设置入口点
    write_csr(sepc, proc->trapframe->epc);

    // 切换到用户模式
    return_to_user(proc->trapframe);
}
```

切换过程：

1. 设置 S-Mode 中断处理向量
2. 配置陷阱帧，保存内核栈指针和陷阱处理函数地址
3. 设置 `sstatus` 寄存器，准备切换到用户模式
4. 设置 `sepc` 寄存器为程序入口点
5. 执行 `sret` 指令，切换到用户模式

## 8. 用户程序执行

用户程序开始执行，以 `app0.c` 为例：

```c
#include "user_lib.h"
#include "util/types.h"

int main(void) {
  printu(">>> app0 is expected to be executed by hart0\n");
  exit(0);
}
```

当调用 `printu()` 或 `exit()` 时，会触发系统调用：

```c
int do_user_call(uint64 sysnum, uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5, uint64 a6,
                 uint64 a7) {
  int ret;
  asm volatile(
      "ecall\n"      // 触发环境调用指令
      "sw a0, %0"
      : "=m"(ret)
      :
      : "memory");
  return ret;
}
```

## 9. 系统调用处理

当执行 `ecall` 指令时，会触发陷入（trap），进入 `kernel/strap.c` 中的陷阱处理函数：

```c
void smode_trap_handler(uint64 epc, uint64 cause, riscv_regs* r) {
  // 区分不同类型的陷入原因
  if (cause == CAUSE_USER_ECALL) {
    // 用户态系统调用
    r->a0 = do_syscall(r->a0, r->a1, r->a2, r->a3, r->a4, r->a5, r->a6, r->a7);
    r->epc += 4;  // 更新程序计数器
  }
  // ... 其他类型的陷入处理
}
```

然后调用 `kernel/syscall.c` 中的具体系统调用实现：

```c
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
  switch (a0) {
    case SYS_user_print:
      return sys_user_print((const char*)a1, a2);
    case SYS_user_exit:
      return sys_user_exit(a1);
    default:
      panic("Unknown syscall %ld \n", a0);
  }
}
```

## 10. 总结

整个执行流程如下：

1. **Spike 启动** → 加载内核和应用程序
2. **M-Mode 初始化** → 设置基本硬件环境
3. **切换到 S-Mode** → 进入操作系统内核
4. **解析命令行参数** → 获取要执行的应用程序列表
5. **加载第一个应用** → 将 ELF 文件加载到内存
6. **切换到用户模式** → 开始执行用户程序
7. **执行用户代码** → 运行应用程序逻辑
8. **系统调用处理** → 处理用户程序请求（如打印、退出等）
9. **程序结束** → 当调用 exit() 时关闭系统

注意：当前版本的 PKE 只支持顺序执行应用程序，在实际使用中只会运行 `app0`，而不会运行 `app1`。要在多核环境中运行多个应用程序，需要更复杂的调度机制。

## 11. 内存布局

- **内核内存布局**：内核链接脚本 `kernel/kernel.lds` 定义内核从地址 `0x80000000` 开始加载
- **用户程序内存布局**：
    - `app0` 链接脚本 `user/user0.lds` 定义其从地址 `0x81000000` 开始
    - `app1` 链接脚本 `user/user1.lds` 定义其从地址 `0x85000000` 开始
- **内核和用户程序共享同一物理内存空间**，但在 Bare 模式下直接映射，虚拟地址等于物理地址