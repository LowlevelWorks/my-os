#include "terminal.h"
#include <stdint.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

#define DEFAULT_COLOR 0x07

static unsigned int row = 0;
static unsigned int column = 0;
static uint8_t color = DEFAULT_COLOR;

static void terminal_newline(void) {
  column = 0;
  row++;

  if (row >= VGA_HEIGHT) {
    row = 0;
  }
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
}

void terminal_putchar(char c) {
  if (c == '\n') {
    terminal_newline();
    return;
  }

  VGA_MEMORY[row * VGA_WIDTH + column] = ((uint16_t)color << 8) | (uint8_t)c;

  column++;

  if (column >= VGA_WIDTH) {
    terminal_newline();
  }
}

void terminal_backspace(void) {
  if (column == 0) {
    return;
  }

  column--;

  VGA_MEMORY[row * VGA_WIDTH + column] = ((uint16_t)color << 8) | ' ';
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
}
