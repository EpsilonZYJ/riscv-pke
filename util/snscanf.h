//
// Created by 周煜杰 on 2026/3/10.
//

#ifndef RISCV_PKE_SNSCANF_H
#define RISCV_PKE_SNSCANF_H

#include <stdarg.h>

#include "types.h"

int vsnscanf(char *in, size_t n, const char *s, va_list vl);

#endif // RISCV_PKE_SNSCANF_H
