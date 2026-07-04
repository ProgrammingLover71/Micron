#include "edit.h"
#include "../../kbd/poll.h"
#include "../../vga/vga.h"
#include "../../fs/fat32.h"
#include "../../shell/shell_str.h"
#include <stdint.h>

#define EDIT_MAX_LINES 128
#define EDIT_LINE_LEN  78
#define EDIT_VIEW_ROWS 23
#define EDIT_FILE_MAX  4096

typedef enum {
    EDIT_NORMAL = 0,
    EDIT_INSERT,
    EDIT_COMMAND
} edit_mode_t;

static char lines[EDIT_MAX_LINES][EDIT_LINE_LEN + 1];
static int line_count;
static int cx;
static int cy;
static int top_line;
static int dirty;
static edit_mode_t mode;
static char filename[13];
static char status[64];
static char command[16];
static int command_len;
static int pending_d;

static int str_len(const char *s)
{
    int n = 0;
    while (s[n])
        ++n;
    return n;
}

static void str_copy_local(char *dst, const char *src, int max)
{
    int i = 0;
    if (max <= 0)
        return;
    while (src && src[i] && i < max - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void set_status(const char *s)
{
    str_copy_local(status, s, sizeof(status));
}

static void clear_buffer(void)
{
    for (int y = 0; y < EDIT_MAX_LINES; ++y)
        lines[y][0] = '\0';
    line_count = 1;
    cx = cy = top_line = 0;
    dirty = 0;
}

static void clamp_cursor(void)
{
    if (line_count < 1)
        line_count = 1;
    if (cy < 0)
        cy = 0;
    if (cy >= line_count)
        cy = line_count - 1;

    int len = str_len(lines[cy]);
    if (cx < 0)
        cx = 0;
    if (cx > len)
        cx = len;

    if (cy < top_line)
        top_line = cy;
    if (cy >= top_line + EDIT_VIEW_ROWS)
        top_line = cy - EDIT_VIEW_ROWS + 1;
}

static void load_file(const char *name)
{
    static uint8_t data[EDIT_FILE_MAX];
    uint32_t bytes = 0;

    clear_buffer();
    str_copy_local(filename, name, sizeof(filename));

    if (!name || !name[0]) {
        str_copy_local(filename, "UNTITLED.TXT", sizeof(filename));
        set_status("new buffer");
        return;
    }

    if (!fat32_is_mounted() || !fat32_read_file(name, data, sizeof(data), &bytes)) {
        set_status("new buffer");
        return;
    }

    line_count = 1;
    int col = 0;
    for (uint32_t i = 0; i < bytes && line_count < EDIT_MAX_LINES; ++i) {
        char c = (char)data[i];
        if (c == '\r')
            continue;
        if (c == '\n') {
            lines[line_count - 1][col] = '\0';
            ++line_count;
            col = 0;
            continue;
        }
        if (col < EDIT_LINE_LEN)
            lines[line_count - 1][col++] = c;
    }
    lines[line_count - 1][col] = '\0';
    set_status("file loaded");
}

static int serialize(uint8_t *out, uint32_t max, uint32_t *bytes)
{
    uint32_t pos = 0;
    for (int y = 0; y < line_count; ++y) {
        int len = str_len(lines[y]);
        for (int x = 0; x < len; ++x) {
            if (pos >= max)
                return 0;
            out[pos++] = (uint8_t)lines[y][x];
        }
        if (y + 1 < line_count) {
            if (pos >= max)
                return 0;
            out[pos++] = '\n';
        }
    }
    *bytes = pos;
    return 1;
}

static int save_file(void)
{
    static uint8_t data[EDIT_FILE_MAX];
    uint32_t bytes = 0;

    if (!serialize(data, sizeof(data), &bytes)) {
        set_status("file too large");
        return 0;
    }

    if (!fat32_write_existing_file(filename, data, bytes)) {
        set_status("save failed: existing FAT32 file only");
        return 0;
    }

    dirty = 0;
    set_status("written");
    return 1;
}

static void render_line(const char *s)
{
    int x = 0;
    while (s[x] && x < VGA_WIDTH) {
        vga_putc(s[x]);
        ++x;
    }
    while (x++ < VGA_WIDTH)
        vga_putc(' ');
}

static int render_limited(const char *s, int limit)
{
    int n = 0;
    while (s[n] && n < limit) {
        vga_putc(s[n]);
        ++n;
    }
    return n;
}

static void render(void)
{
    int written = 0;

    vga_clear();

    for (int row = 0; row < EDIT_VIEW_ROWS; ++row) {
        int idx = top_line + row;
        if (idx < line_count) {
            render_line(lines[idx]);
        } else {
            vga_putc('~');
            for (int x = 1; x < VGA_WIDTH; ++x)
                vga_putc(' ');
        }
    }

    vga_set_cursor(0, EDIT_VIEW_ROWS);
    vga_set_color(0, 7);
    if (mode == EDIT_INSERT) {
        written += render_limited("-- INSERT -- ", VGA_WIDTH - written);
    } else if (mode == EDIT_COMMAND) {
        written += render_limited(":", VGA_WIDTH - written);
    } else {
        written += render_limited("-- NORMAL -- ", VGA_WIDTH - written);
    }

    if (mode == EDIT_COMMAND) {
        written += render_limited(command, VGA_WIDTH - written);
    } else {
        if (dirty)
            written += render_limited(filename, VGA_WIDTH - written);
        else
            written += render_limited(filename, VGA_WIDTH - written);
        if (dirty)
            written += render_limited(" [+]", VGA_WIDTH - written);
        written += render_limited("  ", VGA_WIDTH - written);
        written += render_limited(status, VGA_WIDTH - written);
    }

    while (written++ < VGA_WIDTH)
        vga_putc(' ');
    vga_set_color(15, 0);

    clamp_cursor();
    vga_set_cursor(cx, cy - top_line);
}

static void insert_char(char c)
{
    int len = str_len(lines[cy]);
    if (len >= EDIT_LINE_LEN)
        return;

    for (int i = len; i >= cx; --i)
        lines[cy][i + 1] = lines[cy][i];
    lines[cy][cx++] = c;
    dirty = 1;
}

static void delete_char(void)
{
    int len = str_len(lines[cy]);
    if (cx >= len)
        return;

    for (int i = cx; i < len; ++i)
        lines[cy][i] = lines[cy][i + 1];
    dirty = 1;
}

static void backspace(void)
{
    int len;
    if (cx > 0) {
        --cx;
        delete_char();
        return;
    }

    if (cy == 0)
        return;

    len = str_len(lines[cy - 1]);
    int cur_len = str_len(lines[cy]);
    if (len + cur_len > EDIT_LINE_LEN)
        return;

    for (int i = 0; i <= cur_len; ++i)
        lines[cy - 1][len + i] = lines[cy][i];

    for (int y = cy; y < line_count - 1; ++y)
        str_copy_local(lines[y], lines[y + 1], EDIT_LINE_LEN + 1);

    --line_count;
    --cy;
    cx = len;
    dirty = 1;
}

static void newline(void)
{
    if (line_count >= EDIT_MAX_LINES)
        return;

    int len = str_len(lines[cy]);
    for (int y = line_count; y > cy + 1; --y)
        str_copy_local(lines[y], lines[y - 1], EDIT_LINE_LEN + 1);

    int n = 0;
    for (int i = cx; i <= len; ++i)
        lines[cy + 1][n++] = lines[cy][i];
    lines[cy][cx] = '\0';

    ++line_count;
    ++cy;
    cx = 0;
    dirty = 1;
}

static void delete_line(void)
{
    if (line_count == 1) {
        lines[0][0] = '\0';
        cx = 0;
        dirty = 1;
        return;
    }

    for (int y = cy; y < line_count - 1; ++y)
        str_copy_local(lines[y], lines[y + 1], EDIT_LINE_LEN + 1);
    --line_count;
    dirty = 1;
    clamp_cursor();
}

static void normal_key(kbd_event_t *ev, int *done)
{
    if (ev->type == KBD_KEY_LEFT || (ev->type == KBD_KEY_CHAR && ev->ch == 'h'))
        --cx;
    else if (ev->type == KBD_KEY_RIGHT || (ev->type == KBD_KEY_CHAR && ev->ch == 'l'))
        ++cx;
    else if (ev->type == KBD_KEY_UP || (ev->type == KBD_KEY_CHAR && ev->ch == 'k'))
        --cy;
    else if (ev->type == KBD_KEY_DOWN || (ev->type == KBD_KEY_CHAR && ev->ch == 'j'))
        ++cy;
    else if (ev->type == KBD_KEY_CHAR && ev->ch == 'i')
        mode = EDIT_INSERT;
    else if (ev->type == KBD_KEY_CHAR && ev->ch == 'a') {
        ++cx;
        mode = EDIT_INSERT;
    } else if (ev->type == KBD_KEY_CHAR && ev->ch == 'o') {
        cx = str_len(lines[cy]);
        newline();
        mode = EDIT_INSERT;
    } else if (ev->type == KBD_KEY_CHAR && ev->ch == 'x') {
        delete_char();
    } else if (ev->type == KBD_KEY_CHAR && ev->ch == 'd') {
        if (pending_d)
            delete_line();
        pending_d = !pending_d;
        return;
    } else if (ev->type == KBD_KEY_CHAR && ev->ch == ':') {
        mode = EDIT_COMMAND;
        command_len = 0;
        command[0] = '\0';
    } else if (ev->type == KBD_KEY_ESCAPE) {
        pending_d = 0;
    }

    (void)done;
    pending_d = 0;
}

static void insert_key(kbd_event_t *ev)
{
    if (ev->type == KBD_KEY_ESCAPE) {
        mode = EDIT_NORMAL;
        return;
    }
    if (ev->type == KBD_KEY_BACKSPACE) {
        backspace();
        return;
    }
    if (ev->type == KBD_KEY_ENTER) {
        newline();
        return;
    }
    if (ev->type == KBD_KEY_CHAR && ev->ch >= ' ')
        insert_char(ev->ch);
}

static void command_key(kbd_event_t *ev, int *done)
{
    if (ev->type == KBD_KEY_ESCAPE) {
        mode = EDIT_NORMAL;
        return;
    }
    if (ev->type == KBD_KEY_BACKSPACE) {
        if (command_len > 0)
            command[--command_len] = '\0';
        return;
    }
    if (ev->type == KBD_KEY_ENTER) {
        if (str_eq(command, "w")) {
            save_file();
        } else if (str_eq(command, "q")) {
            if (dirty)
                set_status("unsaved changes; use :q!");
            else
                *done = 1;
        } else if (str_eq(command, "q!")) {
            *done = 1;
        } else if (str_eq(command, "wq")) {
            if (save_file())
                *done = 1;
        } else {
            set_status("unknown command");
        }
        mode = EDIT_NORMAL;
        return;
    }
    if (ev->type == KBD_KEY_CHAR && command_len < (int)sizeof(command) - 1) {
        command[command_len++] = ev->ch;
        command[command_len] = '\0';
    }
}

int edit_run(const char *name)
{
    kbd_event_t ev;
    int done = 0;

    mode = EDIT_NORMAL;
    command_len = 0;
    pending_d = 0;
    load_file(name);

    while (!done) {
        render();
        if (!keyboard_read_event(&ev) || !ev.pressed)
            continue;

        if (mode == EDIT_INSERT)
            insert_key(&ev);
        else if (mode == EDIT_COMMAND)
            command_key(&ev, &done);
        else
            normal_key(&ev, &done);

        clamp_cursor();
    }

    vga_clear();
    return 0;
}
