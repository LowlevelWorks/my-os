#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define KEY_ARROW_UP 0x100
#define KEY_ARROW_DOWN 0x101
#define KEY_ARROW_LEFT 0x102
#define KEY_ARROW_RIGHT 0x103

#define KEY_HOME 0x104
#define KEY_END 0x105
#define KEY_PAGE_UP 0x106
#define KEY_PAGE_DOWN 0x107

#define KEY_INSERT 0x108
#define KEY_DELETE 0x109
#define KEY_ENTER 0x10A
#define KEY_BACKSPACE 0x10B

#define KEY_F1 0x10C
#define KEY_F2 0x10D
#define KEY_F3 0x10E
#define KEY_F4 0x10F
#define KEY_F5 0x110
#define KEY_F6 0x111
#define KEY_F7 0x112
#define KEY_F8 0x113
#define KEY_F9 0x114
#define KEY_F10 0x115
#define KEY_F11 0x116
#define KEY_F12 0x117

#define KEY_CTRL_C 0x120
#define KEY_CTRL_L 0x121

void keyboard_handler(void);

int keyboard_has_input(void);
uint16_t keyboard_getkey(void);

#endif
