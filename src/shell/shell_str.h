#pragma once

#include <stdint.h>

int str_eq(const char *, const char *);
int str_begins(const char *, const char *);
char *int_to_str(int value, char *buf, int buf_size);