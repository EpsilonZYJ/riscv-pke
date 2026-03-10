# pd 结构体大小的精确计算

## pd 结构体定义

```c
typedef struct pd_t {
    int flag;          // 字段1
    uint64 size;       // 字段2
    struct pd_t *next; // 字段3
} pd;
```

---

## 内存布局分析（64位系统）

### 字段信息

| 字段 | 类型 | 大小 | 对齐要求 |
|------|------|------|---------|
| flag | int | 4 字节 | 4字节 |
| size | uint64 | 8 字节 | 8字节 |
| next | struct pd_t* | 8 字节 | 8字节 |

### 内存对齐规则（RISC-V 64位系统）

结构体的对齐要求等于其**最严格成员的对齐要求**，即 `uint64` 的 8 字节。

### 实际内存布局

```
偏移量    字段         大小    对齐说明
────────────────────────────────────────
0x00      flag (int)    4      占用 0x00-0x03
0x04      [填充]        4      对齐到下一个 8 字节边界
0x08      size (uint64) 8      占用 0x08-0x0f，8字节对齐✓
0x10      next (ptr)    8      占用 0x10-0x17，8字节对齐✓
0x18      [结束]        -      -

总大小 = 0x18 = 24 字节
```

### 验证公式

C语言结构体大小 = 最后一个字段的偏移量 + 最后一个字段的大小

- 最后字段 (next) 偏移：0x10 (16)
- 最后字段大小：8
- **总大小 = 16 + 8 = 24 字节** ✓

---

## 实验验证方法

创建 `sizeof_test.c`：

```c
#include <stdio.h>

typedef struct pd_t {
    int flag;
    unsigned long size;
    struct pd_t *next;
} pd;

int main() {
    printf("Size of pd struct: %zu bytes\n", sizeof(pd));
    printf("Offset of 'flag': %zu\n", offsetof(pd, flag));
    printf("Offset of 'size': %zu\n", offsetof(pd, size));
    printf("Offset of 'next': %zu\n", offsetof(pd, next));
    return 0;
}
```

**预期输出**：
```
Size of pd struct: 24 bytes
Offset of 'flag': 0
Offset of 'size': 8
Offset of 'next': 16
```

---

## 在 RISC-V PKE 中的验证

可以在内核代码中添加 static_assert 来验证：

```c
// process.h 中添加
// 验证 pd 结构体大小
_Static_assert(sizeof(pd) == 24, "pd struct must be 24 bytes for memory allocation to work correctly");
```

或者在 syscall.c 中添加运行时检查：

```c
void check_pd_size() {
    if (sizeof(pd) != 24) {
        panic("Unexpected pd struct size: %lx (expected 24)\n", sizeof(pd));
    }
    sprint("pd struct size verified: %lx bytes\n", sizeof(pd));
}
```

---

## PGSIZE - sizeof(pd) 的计算

假设 PGSIZE = 4096（常见的页大小）：

```
一页的总大小      = 4096 字节 (0x1000)
pd 结构体大小     = 24 字节 (0x18)

每页的有效数据空间 = 4096 - 24 = 4072 字节 (0x0fe8)
```

### 当申请 better_malloc(4096) 时

```
申请大小        = 4096
pd 开销          = 24
总需求          = 4096 + 24 = 4120 字节

第1页可用空间   = 4096
是否足够?        = 4120 > 4096? → NO，需要额外页面

计算额外页数量：
remaining = 4120 - 4096 = 24 字节
extra_pages = ceil(24 / 4096) = 1

所以需要分配 2 页 (8192 字节) 来满足 4120 字节的需求
```

---

## 内存浪费分析

### 跨页分配的浪费

当 `better_malloc(size)` 其中 `size + 24 > 4096` 时：

```
假设 size = 4096:
┌──────────┬──────────┐
│  第1页   │  第2页   │
│ 4096字节 │4096字节  │
└──────────┴──────────┘

对象在第1页的布局：
┌────────┬──────────────────┐
│ pd(24) │  用户数据(4096)  │ → 总共 4120 字节
└────────┴──────────────────┘
│←24→│←需要延伸到第2页→

第2页的使用：
┌──────────────────────────┐
│  用户数据继续(24字节)     │← 只用了 24 字节！
│ [空闲：4072字节]          │
└──────────────────────────┘

浪费：4072 字节 / 4096 字节 = 99.4% 空间浪费！
```

### 内存利用率

| 申请大小 | pd开销 | 需要页数 | 分配大小 | 利用率 |
|---------|--------|---------|---------|-------|
| 100 | 24 | 1 | 4096 | 2.4% |
| 1000 | 24 | 1 | 4096 | 24.4% |
| 4096 | 24 | 2 | 8192 | 50.1% |
| 8000 | 24 | 2 | 8192 | 97.7% |
| 8192 | 24 | 3 | 12288 | 66.8% |

---

## 总结

| 项目 | 值 | 说明 |
|------|-----|------|
| pd 结构体大小 | 24 字节 | 含4+4(padding)+8+8 |
| PGSIZE | 4096 字节 | 标准页大小 |
| 每页有效空间 | 4072 字节 | PGSIZE - sizeof(pd) |
| better_malloc(4096)需求 | 4120 字节 | 4096 + 24 |
| 分配页数 | 2 | ceil(4120/4096) |
| 最终占用 | 8192 字节 | 2 × 4096 |
| **空间浪费** | **49.9%** | 约一半的物理页被浪费 |

---

## 关键启示

✓ **申请 PGSIZE 字节时，得到的实际可用大小 < 4096 字节**

• 原因 1：pd 头部占用 sizeof(pd) = 24 字节
• 原因 2：跨页分配导致第2页几乎全部浪费
• 结果：内存利用率严重下降

这是权衡的代价：为了支持任意大小的内存分配，不得不牺牲内存利用率。
