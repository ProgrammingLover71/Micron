#include "shell_str.h"


int str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}


int str_begins(const char *str, const char *prefix)
{
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++;
        prefix++;
    }
    return 1;
}

char *int_to_str(int value, char *buf, int buf_size)
{
    char tmp[32];
    int len = 0;
    int is_negative = 0;

    if (!buf || buf_size <= 1)
        return 0;

    if (value < 0) {
        is_negative = 1;
        value = -value;
    }

    do {
        tmp[len++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value > 0 && len < (int)sizeof(tmp));

    if (is_negative && len < (int)sizeof(tmp))
        tmp[len++] = '-';

    if (len >= buf_size)
        return 0;

    for (int i = 0; i < len; ++i)
        buf[i] = tmp[len - 1 - i];

    buf[len] = '\0';
    return buf;
}