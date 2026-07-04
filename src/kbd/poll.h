#pragma once

#include <stdint.h>
#include "../io/io.h"

typedef enum {
    KBD_KEY_NONE = 0,
    KBD_KEY_CHAR,
    KBD_KEY_ENTER,
    KBD_KEY_BACKSPACE,
    KBD_KEY_TAB,
    KBD_KEY_ESCAPE,
    KBD_KEY_UP,
    KBD_KEY_DOWN,
    KBD_KEY_LEFT,
    KBD_KEY_RIGHT,
    KBD_KEY_HOME,
    KBD_KEY_END,
    KBD_KEY_DELETE
} kbd_key_t;

typedef struct {
    kbd_key_t type;
    char ch;            // valid when type == KBD_KEY_CHAR
    uint8_t pressed;    // 1 = press, 0 = release
    uint8_t shift;
    uint8_t ctrl;
    uint8_t alt;
    uint8_t scancode;
} kbd_event_t;

void keyboard_init(void);
int keyboard_read_event(kbd_event_t *ev);
char keyboard_getchar(void);