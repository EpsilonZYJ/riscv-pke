# 申请一页大小内容时实际大小分析

## 问题陈述

当用户 `better_malloc(PGSIZE)` 时，实际能得到的可用空间是否等于 PGSIZE？

---

## 关键发现：**实际得到的大小 < PGSIZE**

### 1. pd 结构体占用的空间

首先，看 `process.h` 中的 pd 结构体定义：

```c
typedef struct pd_t {
    int flag;          // 4 字节
    uint64 size;       // 8 字节
    struct pd_t *next; // 8 字节（64位指针）
} pd;  // 总计：20 字节（考虑可能的对齐，可能是 24 或 32 字节）
```

**pd 结构体大小**：在 64 位系统上，根据对齐规则，通常是 **24 字节**（3个字段：4+8+8，由于对齐可能是 4→8结构）。

---

## 2. 每页的实际可用空间

### 在 syscall.c 第 173 行：

```c
pd *new_free_block = (pd *)pa;
new_free_block->flag = 0;
new_free_block->size = PGSIZE - sizeof(pd);  // ← 关键！
```

**结论**：每当分配一个新的堆页时，其可用空间是 `PGSIZE - sizeof(pd)`

例如，在 PGSIZE = 4096 字节的系统上：
- 可用空间 = 4096 - sizeof(pd) = 4096 - 24 = **4072 字节**

---

## 3. 申请 PGSIZE(4096字节)时发生什么

### 关键代码路径：syscall.c 第 95 行

```c
uint64 sys_user_allocate_mem(int n) {
    // ...
    
    // ✓ 关键条件判断
    if (n + sizeof(pd) > PGSIZE - sizeof(pd)) {
        // 进入跨页分配路径！
        // 条件展开：4096 + 24 > 4096 - 24 → 4120 > 4072 ✓ 成立
        
        // 跨页分配...
    }
    
    // 单页内分配
    uint64 alloc_pa = current->mem_rib.alloc(n, ...);
    if (alloc_pa == (uint64)NULL) {
        // 分配新页
        void *pa = alloc_page();
        uint64 alloc_va = current->user_heap_top;
        user_vm_map(...);
        current->user_heap_top += PGSIZE;
        
        // 将新页的剩余空间加入空闲链表
        pd *new_free_block = (pd *)pa;
        new_free_block->size = PGSIZE - sizeof(pd);  // 4072
        insert_free_block(&current->free_list, new_free_block, ...);
        
        // 重新尝试分配
        alloc_pa = current->mem_rib.alloc(n, ...);
    }
}
```

### 具体场景分析

**申请 4096 字节时的实际流程**：

```
调用: better_malloc(4096)
    ↓
sys_user_allocate_mem(4096)
    ↓
检查条件: 4096 + sizeof(pd) > PGSIZE - sizeof(pd)
         4096 + 24 > 4096 - 24
         4120 > 4072 ✓ 成立
    ↓
进入跨页分配路径（需要多页来容纳 4096 + pd头部）
    ↓
实际分配的空间：某个物理地址，指向的是跨页的内存块

但是，用户拿到的返回值是：start_va + sizeof(pd)
这意味着：用户数据前面有一个 sizeof(pd) 的 pd 结构
```

---

## 4. 具体的大小关系

### 假设：PGSIZE = 4096, sizeof(pd) = 24

| 操作 | 计算 | 结果 |
|------|------|------|
| 用户申请 | `better_malloc(4096)` | 4096 字节 |
| 条件判断 | `4096 + 24 > 4096 - 24` | `4120 > 4072` ✓ |
| 路径 | 进入跨页分配 | - |
| 实际需要页数 | `ceil((4096 + 24) / 4096) = ceil(4120/4096) = 2` | **需要2页** |
| 用户能用的最大空间 | 取决于分配器策略 | ≤ 2 × 4096 - 24 = **8168 字节** |

### 问题的核心

1. **用户名义上申请 4096 字节**
2. **实际分配器分配了 2 页 (8192 字节) 的物理页**
3. **但用户能可靠访问的空间 < 4096 字节**（因为 pd 覆盖了 sizeof(pd) 的空间）

---

## 5. 跨页分配的细节分析

在 syscall.c 第 111-157 行，跨页分配的实现：

```c
// 总共需要的空间
uint64 total_needed = sizeof(pd) + n;  // 24 + 4096 = 4120

// start_page 的可用空间
uint64 available_in_first_page = PGSIZE - (start_pa - start_page_base);  // 4096

// 如果第一个页面不够，分配额外页面
if (total_needed > available_in_first_page) {  // 4120 > 4096
    uint64 remaining = total_needed - available_in_first_page;  // 24
    uint64 extra_pages = (remaining + PGSIZE - 1) / PGSIZE;     // ceil(24/4096) = 1
    
    // 分配 1 个额外的页
    for (uint64 i = 0; i < extra_pages; i++) {  // i=0
        void *pa = alloc_page();  // 分配第二页
        // 映射到虚拟地址空间
        current->user_heap_top += PGSIZE;
    }
}

// 返回数据地址（pd 头部之后）
return start_va + sizeof(pd);  // 跳过 24 字节的 pd 头部
```

### 结论

- **需要 2 页（8192 字节）的物理页面**
- **但第一页中的前 24 字节被 pd 占用**
- **用户只能访问 8192 - 24 = 8168 字节的可靠范围内的内存**

---

## 6. 比较：naive_malloc vs better_malloc

### naive_malloc(PGSIZE) - 原始实现

```c
uint64 sys_user_allocate_page() {
    // 原始实现（已在代码中改为）
    return sys_user_allocate_mem(PGSIZE);  // 现在转发给 better_malloc
}
```

如果是原始设计（直接分配）：
```c
void *pa = alloc_page();
va = current->user_heap.heap_top;
user_vm_map(..., va, PGSIZE, pa, ...);  // 分配整个页面给用户
return va;
```

**原始的 naive_malloc(PGSIZE)**：
- 分配 1 页
- 用户得到的 VA 指向 1 页的起始
- 用户可用空间 = 4096 字节 ✓

### better_malloc(PGSIZE) - 当前实现

- 分配 2 页
- 用户得到的 VA 中前 sizeof(pd) 字节被占用
- 用户可用空间 = 8192 - 24 = 8168 字节（但分配器可能返回了少于这个数值）

---

## 7. 实际的数据存储问题

### 假设 better_malloc(4096)

```
分配的虚拟地址空间:
┌─────────────────┐─────────────────┐
│    第1页        │     第2页        │
├─────────────────┼─────────────────┤
0x100000         0x101000         0x102000

pd 结构体位置: 0x100000
  ↓
  ├─ flag: 0x100000 (4字节)
  ├─ size: 0x100004 (8字节)
  └─ next: 0x10000c (8字节)

用户数据地址: 0x100018 (= 0x100000 + sizeof(pd) = 0x100000 + 24)
  ↓
  用户可访问: 0x100018 ~ 0x1fffff98
  (从第1页的56字节到第2页页尾)
  
实际可用大小：
  = (0x101000 - 0x100018) + 0x101000
  = (4096 - 24) + 4096
  = 4072 + 4096
  = 8168 字节
```

---

## 8. 关键问题总结

| 问题 | 原因 | 后果 |
|------|------|------|
| 申请 PGSIZE，实际得到 < PGSIZE | pd 结构体要占用 sizeof(pd) 字节 | **静默内存不足** |
| 不是真的得到 PGSIZE | 分配器没有透明地隐藏 pd | 用户需要知道内存开销 |
| 跨页分配会浪费页面 | 4120 字节需要 2 页 | **内存利用率低** |

---

## 9. 具体的实验验证方法

创建测试程序：

```c
#include "user_lib.h"

#define PGSIZE 4096

int main(void) {
    // 测试1：申请恰好PGSIZE大小
    char *buf = (char *)better_malloc(PGSIZE);
    
    if (buf != NULL) {
        // 尝试填充整个 PGSIZE
        for (int i = 0; i < PGSIZE; i++) {
            buf[i] = 'A';  // 如果大小确实不足，可能段错误或覆盖其他数据
        }
        printu("Successfully wrote PGSIZE bytes\n");
        
        // 检查是否超过了安全范围
        // 后续的 malloc 会看到数据被破坏吗？
        char *buf2 = (char *)better_malloc(100);
        if (buf2 == NULL) {
            printu("Allocation failed! Memory exhausted\n");
        } else {
            printu("Second allocation succeeded at 0x%lx\n", (uint64)buf2);
        }
    }
    
    exit(0);
}
```

---

## 10. 结论

**是的，申请一页大小的内容时，实际得到的可用大小 < PGSIZE**

具体来说：
- **理想情况**：`better_malloc(PGSIZE)` 应该返回能访问 PGSIZE 字节的内存
- **实际情况**：由于 pd 结构体的存在和跨页分配的开销，用户实际可用的空间受到以下限制：
  1. **每个物理页中被 pd 占用 sizeof(pd) 字节**
  2. **跨页分配时会浪费页面**（4120字节需要2页）
  3. **用户无法获得完整的 PGSIZE 字节**

这是设计上的权衡：用元数据来管理内存的灵活性，代价是内存利用率下降和用户得不到期望的字节数。
