#include "vga.h"

static volatile uint16_t *const VGA_MEM = (volatile uint16_t *)0xB8000;

static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t vga_color = 0x0F; // white on black

static inline uint16_t vga_entry(char c, uint8_t color)
{
    return ((uint16_t)color << 8) | (uint8_t)c;
}

static void vga_update_cursor(void)
{
    uint16_t pos = (uint16_t)(cursor_y * VGA_WIDTH + cursor_x);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));

    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void vga_scroll(void)
{
    if (cursor_y < VGA_HEIGHT)
        return;

    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_MEM[(y - 1) * VGA_WIDTH + x] = VGA_MEM[y * VGA_WIDTH + x];
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_MEM[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color);
    }

    cursor_y = VGA_HEIGHT - 1;
}

void vga_init(void)
{
    vga_clear();
}

void vga_set_color(uint8_t fg, uint8_t bg)
{
    vga_color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

void vga_set_cursor(int x, int y)
{
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= VGA_WIDTH)
        x = VGA_WIDTH - 1;
    if (y >= VGA_HEIGHT)
        y = VGA_HEIGHT - 1;

    cursor_x = x;
    cursor_y = y;
    vga_update_cursor();
}

void vga_clear(void)
{
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEM[i] = vga_entry(' ', vga_color);
    }

    cursor_x = 0;
    cursor_y = 0;
    vga_update_cursor();
}

void vga_putc(char c)
{
    switch (c) {
        case '\n':
            cursor_x = 0;
            cursor_y++;
            break;

        case '\r':
            cursor_x = 0;
            break;

        case '\b':
            if (cursor_x > 0) {
                cursor_x--;
                VGA_MEM[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', vga_color);
            }
            break;

        default:
            VGA_MEM[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, vga_color);
            cursor_x++;

            if (cursor_x >= VGA_WIDTH) {
                cursor_x = 0;
                cursor_y++;
            }
            break;
    }

    vga_scroll();
    vga_update_cursor();
}

void vga_puts(const char *s)
{
    while (*s) {
        vga_putc(*s++);
    }
}
