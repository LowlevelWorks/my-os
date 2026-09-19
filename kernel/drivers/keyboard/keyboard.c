#include "keyboard.h"
#include "io.h"
#include "keyboard_map.h"
#include <stdint.h>

#define KEYBOARD_BUFFER_SIZE 256
#define KEYBOARD_DATA_PORT 0x60

#define SCANCODE_LSHIFT 0x2A
#define SCANCODE_RSHIFT 0x36
#define SCANCODE_LCTRL 0x1D
#define SCANCODE_LALT 0x38
#define SCANCODE_CAPSLOCK 0x3A

#define SCANCODE_EXT_ARROW_UP 0x48
#define SCANCODE_EXT_ARROW_DOWN 0x50
#define SCANCODE_EXT_ARROW_LEFT 0x4B
#define SCANCODE_EXT_ARROW_RIGHT 0x4D

#define SCANCODE_EXT_HOME 0x47
#define SCANCODE_EXT_END 0x4F
#define SCANCODE_EXT_PAGE_UP 0x49
#define SCANCODE_EXT_PAGE_DOWN 0x51
#define SCANCODE_EXT_INSERT 0x52
#define SCANCODE_EXT_DELETE 0x53

static uint16_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];

static volatile unsigned int buffer_head = 0;
static volatile unsigned int buffer_tail = 0;

static int extended_prefix = 0;

static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int alt_pressed = 0;
static int caps_lock = 0;

static void push_key(uint16_t key) {
  unsigned int next = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;

  if (next == buffer_tail) {
    return;
  }

  keyboard_buffer[buffer_head] = key;
  buffer_head = next;
}

static int handle_modifier(uint8_t scancode) {
  uint8_t code = scancode & 0x7F;
  int pressed = !(scancode & 0x80);

  switch (code) {

  case SCANCODE_LSHIFT:
  case SCANCODE_RSHIFT:
    shift_pressed = pressed;
    return 1;

  case SCANCODE_LCTRL:
    ctrl_pressed = pressed;
    return 1;

  case SCANCODE_LALT:
    alt_pressed = pressed;
    return 1;

  case SCANCODE_CAPSLOCK:
    if (pressed) {
      caps_lock = !caps_lock;
    }
    return 1;

  default:
    return 0;
  }
}

static char scancode_to_char(uint8_t scancode) {
  char c;

  if (scancode >= sizeof(keyboard_map)) {
    return 0;
  }

  if (shift_pressed) {
    c = keyboard_map_shift[scancode];
  } else {
    c = keyboard_map[scancode];
  }

  if (c >= 'a' && c <= 'z') {
    if (shift_pressed ^ caps_lock) {
      c = c - 'a' + 'A';
    }
  }

  else if (c >= 'A' && c <= 'Z') {
    if (shift_pressed && caps_lock) {
      c = c - 'A' + 'a';
    }
  }

  return c;
}

static void handle_extended_scancode(uint8_t scancode) {
  uint8_t code = scancode & 0x7F;
  int pressed = !(scancode & 0x80);

  if (code == SCANCODE_LCTRL) {
    ctrl_pressed = pressed;
    return;
  }

  if (code == SCANCODE_LALT) {
    alt_pressed = pressed;
    return;
  }

  if (scancode & 0x80) {
    return;
  }

  switch (scancode) {

  case SCANCODE_EXT_ARROW_UP:
    push_key(KEY_ARROW_UP);
    break;

  case SCANCODE_EXT_ARROW_DOWN:
    push_key(KEY_ARROW_DOWN);
    break;

  case SCANCODE_EXT_ARROW_LEFT:
    push_key(KEY_ARROW_LEFT);
    break;

  case SCANCODE_EXT_ARROW_RIGHT:
    push_key(KEY_ARROW_RIGHT);
    break;

  case SCANCODE_EXT_HOME:
    push_key(KEY_HOME);
    break;

  case SCANCODE_EXT_END:
    push_key(KEY_END);
    break;

  case SCANCODE_EXT_PAGE_UP:
    push_key(KEY_PAGE_UP);
    break;

  case SCANCODE_EXT_PAGE_DOWN:
    push_key(KEY_PAGE_DOWN);
    break;

  case SCANCODE_EXT_INSERT:
    push_key(KEY_INSERT);
    break;

  case SCANCODE_EXT_DELETE:
    push_key(KEY_DELETE);
    break;

  default:
    break;
  }
}

static uint16_t function_key(uint8_t scancode) {
  switch (scancode) {

  case 0x1C:
    return KEY_ENTER;
  case 0x0E:
    return KEY_BACKSPACE;
  case 0x3B:
    return KEY_F1;
  case 0x3C:
    return KEY_F2;
  case 0x3D:
    return KEY_F3;
  case 0x3E:
    return KEY_F4;
  case 0x3F:
    return KEY_F5;
  case 0x40:
    return KEY_F6;
  case 0x41:
    return KEY_F7;
  case 0x42:
    return KEY_F8;
  case 0x43:
    return KEY_F9;
  case 0x44:
    return KEY_F10;
  case 0x57:
    return KEY_F11;
  case 0x58:
    return KEY_F12;

  default:
    return 0;
  }
}

void keyboard_handler(void) {
  uint8_t scancode = inb(KEYBOARD_DATA_PORT);

  if (scancode == 0xE0) {
    extended_prefix = 1;
    return;
  }

  if (extended_prefix) {
    extended_prefix = 0;

    handle_extended_scancode(scancode);
    return;
  }

  if (handle_modifier(scancode)) {
    return;
  }

  if (scancode & 0x80) {
    return;
  }

  uint16_t key = function_key(scancode);

  if (key) {
    push_key(key);
    return;
  }

  char c = scancode_to_char(scancode);

  if (c) {
    push_key((uint16_t)c);
  }
}

int keyboard_has_input(void) { return buffer_head != buffer_tail; }

uint16_t keyboard_getkey(void) {
  uint16_t key = 0;

  if (buffer_head == buffer_tail) {
    return 0;
  }

  key = keyboard_buffer[buffer_tail];

  buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;

  return key;
}
