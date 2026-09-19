#include "shell.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/terminal/terminal.h"
#include "lib/string.h"
#include "parser.h"

#define SHELL_BUFFER_SIZE 256

static char input_buffer[SHELL_BUFFER_SIZE];
static unsigned input_length = 0;

void shell_init(void) { input_length = 0; }

static void shell_process_command(void) {
  char *argv[SHELL_MAX_ARGUMENTS];
  int argc;

  if (!shell_parse_command(input_buffer, &argc, argv)) {
    terminal_write("Too many arguments\n");
    return;
  }

  if (argc == 0)
    return;

  if (string_equals(argv[0], "help")) {
    terminal_write("Available commands:\n");
    terminal_write("help\n");
    terminal_write("echo\n");
    terminal_write("clear\n");
    return;
  }

  if (string_equals(argv[0], "echo")) {
    for (int i = 1; i < argc; i++) {
      terminal_write(argv[i]);

      if (i + 1 < argc) {
        terminal_putchar(' ');
      }
    }
    terminal_putchar('\n');
    return;
  }

  if (string_equals(argv[0], "clear")) {
    terminal_clear();
    return;
  }

  terminal_write("Unknown command: ");
  terminal_write(input_buffer);
  terminal_write("\n");
}

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
    shell_process_command();
    input_length = 0;
    terminal_write("> ");
    return;
  }
}
