#pragma once

#include <stddef.h>
#include <stdint.h>

void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);
