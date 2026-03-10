//
// Created by 周煜杰 on 2026/3/10.
//
#include "snscanf.h"

int32 vsnscanf(char *in, size_t n, const char *s, va_list vl) {
    bool format = FALSE;
    bool longarg = FALSE;
    size_t cnt = 0;
    size_t pos = 0;
    for (; *s; s++) {
        if (format) {
            switch (*s) {
            case 's': {
                char *s_buf = va_arg(vl, char *);
                while (in[pos] == ' ' || in[pos] == '\t') pos++;
                while (in[pos] != '\n' && in[pos] != '\0' && in[pos] != ' ' && in[pos] != '\t') {
                    if (pos < n) {
                        *s_buf = in[pos];
                        s_buf++;
                        pos++;
                    }
                }
                *s_buf = '\0';
                cnt++;
                format = FALSE;
                break;
            }
            case 'l': {
                longarg = TRUE;
                break;
            }
            case 'c': {
                if (pos < n) {
                    char *c_buf = va_arg(vl, char *);
                    *c_buf = in[pos];
                    pos++;
                    cnt++;
                }
                format = FALSE;
                break;
            }
            case 'd': {
                if (pos < n) {
                    while (in[pos] == ' ' || in[pos] == '\t') pos++;
                    long *d_buf = (longarg ? va_arg(vl, long *) : (long *)va_arg(vl, int *));
                    long digits = 0;
                    long sign = 1;
                    if (in[pos] == '-') {
                        sign = -1;
                        pos++;
                    } else if (in[pos] == '+') {
                        pos++;
                    }
                    while (pos < n && in[pos] >= '0' && in[pos] <= '9') {
                        digits = digits * 10 + (in[pos] - '0');
                        pos++;
                    }
                    *d_buf = sign * digits;
                    cnt++;
                }
                format = FALSE;
                longarg = FALSE;
                break;
            }
            default:
                break;
            }
        } else if (*s == '%') {
            format = TRUE;
        } else {
            while (pos < n && (in[pos] == ' ' || in[pos] == '\t')) {
                pos++;
            }
            if (pos >= n || in[pos] != *s) {
                break;
            }
            pos++;
        }
        if (pos >= n) {
            break;
        }
    }
    return cnt;
}