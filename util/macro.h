//
// Created by 周煜杰 on 2026/3/4.
//

#ifndef RISCV_PKE_MACRO_H
#define RISCV_PKE_MACRO_H

// 辅助宏：字符串化
#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

// 辅助宏：标记粘贴
#define CONCAT(a, b) a##b
#define EXPAND_CONCAT(a, b) CONCAT(a, b)

#endif // RISCV_PKE_MACRO_H
