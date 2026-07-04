#pragma once

#include <stdint.h>
#include "../io/io.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_set_cursor(int x, int y);
