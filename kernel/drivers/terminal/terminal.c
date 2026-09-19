#include "terminal.h"
#include "arch/x86_64/io.h"
#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)
#define VGA_INDEX_PORT 0x3D4
#define VGA_DATA_PORT 0x3D5

#define DEFAULT_COLOR 0x07

static unsigned int row = 0;
static unsigned int column = 0;
static uint8_t color = DEFAULT_COLOR;

static void terminal_update_hardware_cursor(void) {
  uint16_t position = row * VGA_WIDTH + column;

  outb(VGA_INDEX_PORT, 0x0F);
  outb(VGA_DATA_PORT, position & 0xFF);

  outb(VGA_INDEX_PORT, 0x0E);
  outb(VGA_DATA_PORT, (position >> 8) & 0xFF);
}

static void terminal_scroll(void) {
  for (unsigned int y = 1; y < VGA_HEIGHT; y++) {
    for (unsigned int x = 0; x < VGA_WIDTH; x++) {
      VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
    }
  }

  for (unsigned int x = 0; x < VGA_WIDTH; x++) {
    VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = ((uint16_t)color << 8) | ' ';
  }
}

static void terminal_newline(void) {
  column = 0;
  row++;

  if (row >= VGA_HEIGHT) {
    terminal_scroll();
    row = VGA_HEIGHT - 1;
  }

  terminal_update_hardware_cursor();
}

void terminal_init(void) {
  row = 0;
  column = 0;
  color = DEFAULT_COLOR;

  for (unsigned int y = 0; y < VGA_HEIGHT; y++) {
    for (unsigned int x = 0; x < VGA_WIDTH; x++) {
      VGA_MEMORY[y * VGA_WIDTH + x] = ((uint16_t)color << 8) | ' ';
    }
  }
  terminal_update_hardware_cursor();
}

void terminal_putchar(char c) {
  if (c == '\n') {
    terminal_newline();
    return;
  }

  if (column >= VGA_WIDTH) {
    terminal_newline();
  }

  VGA_MEMORY[row * VGA_WIDTH + column] = ((uint16_t)color << 8) | (uint8_t)c;

  column++;

  if (column >= VGA_WIDTH) {
    terminal_newline();
    return;
  }
  terminal_update_hardware_cursor();
}

void terminal_backspace(void) {
  if (column == 0) {
    return;
  }

  column--;

  VGA_MEMORY[row * VGA_WIDTH + column] = ((uint16_t)color << 8) | ' ';
  terminal_update_hardware_cursor();
}

void terminal_write(const char *str) {
  while (*str) {
    terminal_putchar(*str);
    str++;
  }
}

void terminal_clear(void) {
  for (unsigned int y = 0; y < VGA_HEIGHT; y++) {
    for (unsigned int x = 0; x < VGA_WIDTH; x++) {
      VGA_MEMORY[y * VGA_WIDTH + x] = ((uint16_t)color << 8) | ' ';
    }
  }
  row = 0;
  column = 0;
  terminal_update_hardware_cursor();
}

void terminal_set_cursor(unsigned int new_column) {
  if (new_column >= VGA_WIDTH) {
    new_column = VGA_WIDTH - 1;
  }
  column = new_column;
  terminal_update_hardware_cursor();
}

void terminal_draw_line(unsigned int start_column, const char *str,
                        unsigned int length, unsigned int old_length,
                        unsigned int cursor_position) {
  terminal_set_cursor(start_column);

  unsigned int clear_length = old_length;

  if (length > clear_length) {
    clear_length = length;
  }

  for (unsigned int i = 0; i < clear_length; i++) {
    terminal_putchar(' ');
  }

  terminal_set_cursor(start_column);

  for (unsigned int i = 0; i < length; i++) {
    terminal_putchar(str[i]);
  }

  terminal_set_cursor(start_column + cursor_position);
}
