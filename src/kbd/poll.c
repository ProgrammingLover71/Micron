#include "poll.h"


static inline void cpu_relax(void)
{
    __asm__ volatile ("pause");
}

static int wait_for_keyboard_data(uint32_t timeout)
{
    while (timeout-- > 0) {
        uint8_t status = inb(0x64);

        if (status & 0x01)
            return 1;

        cpu_relax();
    }

    return 0;
}

static void drain_keyboard_buffer(void)
{
    while (inb(0x64) & 0x01)
        (void)inb(0x60);
}

static uint8_t shift_l = 0;
static uint8_t shift_r = 0;
static uint8_t ctrl_l  = 0;
static uint8_t ctrl_r  = 0;
static uint8_t alt_l   = 0;
static uint8_t alt_r   = 0;
static uint8_t caps    = 0;
static uint8_t num     = 0;
static uint8_t ext     = 0;

static const char normal_map[128] = {
    [0x01] = 0x1B, // Esc
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',
    [0x0C] = '-',
    [0x0D] = '=',
    [0x0E] = '\b',
    [0x0F] = '\t',
    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'y',
    [0x16] = 'u',
    [0x17] = 'i',
    [0x18] = 'o',
    [0x19] = 'p',
    [0x1A] = '[',
    [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',
    [0x27] = ';',
    [0x28] = '\'',
    [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',
    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',
    [0x33] = ',',
    [0x34] = '.',
    [0x35] = '/',
    [0x39] = ' ',
};

static const char shift_map[128] = {
    [0x01] = 0x1B,
    [0x02] = '!',
    [0x03] = '@',
    [0x04] = '#',
    [0x05] = '$',
    [0x06] = '%',
    [0x07] = '^',
    [0x08] = '&',
    [0x09] = '*',
    [0x0A] = '(',
    [0x0B] = ')',
    [0x0C] = '_',
    [0x0D] = '+',
    [0x0E] = '\b',
    [0x0F] = '\t',
    [0x10] = 'Q',
    [0x11] = 'W',
    [0x12] = 'E',
    [0x13] = 'R',
    [0x14] = 'T',
    [0x15] = 'Y',
    [0x16] = 'U',
    [0x17] = 'I',
    [0x18] = 'O',
    [0x19] = 'P',
    [0x1A] = '{',
    [0x1B] = '}',
    [0x1C] = '\n',
    [0x1E] = 'A',
    [0x1F] = 'S',
    [0x20] = 'D',
    [0x21] = 'F',
    [0x22] = 'G',
    [0x23] = 'H',
    [0x24] = 'J',
    [0x25] = 'K',
    [0x26] = 'L',
    [0x27] = ':',
    [0x28] = '"',
    [0x29] = '~',
    [0x2B] = '|',
    [0x2C] = 'Z',
    [0x2D] = 'X',
    [0x2E] = 'C',
    [0x2F] = 'V',
    [0x30] = 'B',
    [0x31] = 'N',
    [0x32] = 'M',
    [0x33] = '<',
    [0x34] = '>',
    [0x35] = '?',
    [0x39] = ' ',
};

static const char numpad_map[128] = {
    [0x47] = '7',
    [0x48] = '8',
    [0x49] = '9',
    [0x4A] = '-',
    [0x4B] = '4',
    [0x4C] = '5',
    [0x4D] = '6',
    [0x4E] = '+',
    [0x4F] = '1',
    [0x50] = '2',
    [0x51] = '3',
    [0x52] = '0',
    [0x53] = '.',
};

static uint8_t shift_active(void)
{
    return (shift_l || shift_r) ? 1 : 0;
}

static uint8_t ctrl_active(void)
{
    return (ctrl_l || ctrl_r) ? 1 : 0;
}

static uint8_t alt_active(void)
{
    return (alt_l || alt_r) ? 1 : 0;
}

static char translate_scancode(uint8_t sc)
{
    if (sc >= 128)
        return 0;

    if (num && numpad_map[sc])
        return numpad_map[sc];

    char base = normal_map[sc];
    char shifted = shift_map[sc];

    if (!base && !shifted)
        return 0;

    if (base >= 'a' && base <= 'z') {
        if (ctrl_active()) {
            return (char)(base - 'a' + 1);
        }
        if ((shift_active() ^ caps) != 0)
            return shifted ? shifted : (char)(base - 32);
        return base;
    }

    if (shift_active())
        return shifted ? shifted : base;

    return base;
}

void keyboard_init(void)
{
    shift_l = shift_r = 0;
    ctrl_l = ctrl_r = 0;
    alt_l = alt_r = 0;
    caps = 0;
    num = 0;
    ext = 0;

    drain_keyboard_buffer();
}

int keyboard_read_event(kbd_event_t *ev)
{
    if (!ev)
        return 0;

    for (;;) {
        if (!wait_for_keyboard_data(100000))
            return 0;

        uint8_t sc = inb(0x60);

        if (sc == 0xE1) {
            continue; // Pause/Break prefix; ignore for now
        }

        if (sc == 0xFA || sc == 0xFE || sc == 0xFC || sc == 0x00) {
            continue; // Ignore controller acknowledgements and empty data
        }

        if (sc == 0xE0) {
            ext = 1;
            continue;
        }

        uint8_t released = (sc & 0x80) ? 1 : 0;
        uint8_t code = sc & 0x7F;

        ev->scancode = sc;
        ev->pressed  = released ? 0 : 1;
        ev->shift    = shift_active();
        ev->ctrl     = ctrl_active();
        ev->alt      = alt_active();
        ev->type     = KBD_KEY_NONE;
        ev->ch       = 0;

        if (ext) {
            ext = 0;

            if (code == 0x1D) { // Right Ctrl
                ctrl_r = !released;
                continue;
            }
            if (code == 0x38) { // Right Alt
                alt_r = !released;
                continue;
            }

            if (released)
                continue;

            switch (code) {
                case 0x47: ev->type = KBD_KEY_HOME;    return 1;
                case 0x48: ev->type = KBD_KEY_UP;      return 1;
                case 0x49: ev->type = KBD_KEY_DELETE;  return 1;
                case 0x4B: ev->type = KBD_KEY_LEFT;    return 1;
                case 0x4D: ev->type = KBD_KEY_RIGHT;   return 1;
                case 0x4F: ev->type = KBD_KEY_END;     return 1;
                case 0x50: ev->type = KBD_KEY_DOWN;    return 1;
                case 0x53: ev->type = KBD_KEY_DELETE;  return 1;
                case 0x1C: ev->type = KBD_KEY_ENTER;    return 1; // keypad Enter
                default: break;
            }

            continue;
        }

        switch (code) {
            case 0x2A: // Left Shift
                shift_l = released ? 0 : 1;
                continue;

            case 0x36: // Right Shift
                shift_r = released ? 0 : 1;
                continue;

            case 0x1D: // Left Ctrl
                ctrl_l = released ? 0 : 1;
                continue;

            case 0x38: // Left Alt
                alt_l = released ? 0 : 1;
                continue;

            case 0x3A: // Caps Lock
                if (!released)
                    caps ^= 1;
                continue;

            case 0x45: // Num Lock
                if (!released)
                    num ^= 1;
                continue;

            case 0x0E:
                if (!released) {
                    ev->type = KBD_KEY_BACKSPACE;
                    return 1;
                }
                continue;

            case 0x0F:
                if (!released) {
                    ev->type = KBD_KEY_TAB;
                    return 1;
                }
                continue;

            case 0x01:
                if (!released) {
                    ev->type = KBD_KEY_ESCAPE;
                    return 1;
                }
                continue;

            case 0x1C:
                if (!released) {
                    ev->type = KBD_KEY_ENTER;
                    return 1;
                }
                continue;

            default:
                break;
        }

        if (released)
            continue;

        char ch = translate_scancode(code);
        if (ch) {
            ev->type = KBD_KEY_CHAR;
            ev->ch = ch;
            return 1;
        }
    }
}

char keyboard_getchar(void)
{
    kbd_event_t ev;

    for (;;) {
        keyboard_read_event(&ev);

        if (!ev.pressed)
            continue;

        switch (ev.type) {
            case KBD_KEY_CHAR:       return ev.ch;
            case KBD_KEY_ENTER:      return '\n';
            case KBD_KEY_BACKSPACE:  return '\b';
            case KBD_KEY_TAB:        return '\t';
            case KBD_KEY_ESCAPE:     return 0x1B;
            default:                 break;
        }
    }
}