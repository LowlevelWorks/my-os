
#include "shell.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/terminal/terminal.h"

#define SHELL_BUFFER_SIZE 256

static char input_buffer[SHELL_BUFFER_SIZE];
static unsigned input_length = 0;

void shell_init(void) { input_length = 0; }

void shell_handle_key(uint16_t key) {
  if (key < 0x100) {
    if (input_length < SHELL_BUFFER_SIZE - 1) {
      input_buffer[input_length++] = (char)(key);
      terminal_putchar((char)key);
    }
    return;
  }

  if (key == KEY_BACKSPACE) {
    if (input_length > 0) {
      input_length--;
      terminal_backspace();
    }

    return;
  }

  if (key == KEY_ENTER) {
    terminal_putchar('\n');
    input_buffer[input_length] = '\0';

    input_length = 0;
    return;
  }
}
