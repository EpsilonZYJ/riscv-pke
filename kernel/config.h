#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "util/macro.h"

// we use only one HART (cpu) in fundamental experiments
#define NCPU 1

// 信号量最大支持数目
#define NSEMAPHORE 16

// interval of timer interrupt. added @lab1_3
#define TIMER_INTERVAL 1000000

// the maximum memory space that PKE is allowed to manage. added @lab2_1
#define PKE_MAX_ALLOWABLE_RAM 128 * 1024 * 1024

// the ending physical address that PKE observes. added @lab2_1
#define PHYS_TOP (DRAM_BASE + PKE_MAX_ALLOWABLE_RAM)

#define MEM_ALLOC_STRATEGY first_fit // 可修改为 best_fit, next_fit 等

// 动态生成函数名宏
#define ALLOC_FUNC EXPAND_CONCAT(MEM_ALLOC_STRATEGY, _alloc)
#define FREE_FUNC EXPAND_CONCAT(MEM_ALLOC_STRATEGY, _free)

#endif
