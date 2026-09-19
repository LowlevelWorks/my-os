#include "shell.h"
#include "commands/commands.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/terminal/terminal.h"
#include "parser.h"

#define SHELL_BUFFER_SIZE 256
#define SHELL_PROMPT_COLUMN 2

static char input_buffer[SHELL_BUFFER_SIZE];
static unsigned int input_length = 0;
static unsigned int cursor_position = 0;

void shell_init(void) {
  input_length = 0;
  cursor_position = 0;
}

static void shell_process_command(void) {
  char *argv[SHELL_MAX_ARGUMENTS];
  int argc;

  if (!shell_parse_command(input_buffer, &argc, argv)) {
    terminal_write("Too many arguments\n");
    return;
  }

  if (argc == 0)
    return;

  shell_execute_command(argc, argv);
}

void shell_handle_key(uint16_t key) {
  if (key < 0x100) {
    if (input_length < SHELL_BUFFER_SIZE - 1) {
      unsigned int old_length = input_length;

      for (unsigned int i = input_length; i > cursor_position; i--) {
        input_buffer[i] = input_buffer[i - 1];
      }
      input_buffer[cursor_position] = (char)(key);
      input_length++;
      cursor_position++;
      terminal_draw_line(SHELL_PROMPT_COLUMN, input_buffer, input_length,
                         old_length, cursor_position);
    }
    return;
  }

  if (key == KEY_BACKSPACE) {
    if (cursor_position > 0) {
      unsigned int old_length = input_length;
      for (unsigned int i = cursor_position - 1; i < input_length - 1; i++) {
        input_buffer[i] = input_buffer[i + 1];
      }

      input_length--;
      cursor_position--;
      terminal_draw_line(SHELL_PROMPT_COLUMN, input_buffer, input_length,
                         old_length, cursor_position);
    }

    return;
  }

  if (key == KEY_DELETE) {
    if (cursor_position < input_length) {
      unsigned int old_length = input_length;

      for (unsigned int i = cursor_position; i < input_length - 1; i++) {
        input_buffer[i] = input_buffer[i + 1];
      }

      input_length--;

      terminal_draw_line(SHELL_PROMPT_COLUMN, input_buffer, input_length,
                         old_length, cursor_position);
    }

    return;
  }

  if (key == KEY_ENTER) {
    terminal_putchar('\n');
    input_buffer[input_length] = '\0';
    shell_process_command();
    input_length = 0;
    cursor_position = 0;
    terminal_write("> ");
    return;
  }

  if (key == KEY_ARROW_LEFT) {
    if (cursor_position > 0) {
      cursor_position--;
      terminal_set_cursor(SHELL_PROMPT_COLUMN + cursor_position);
    }
    return;
  }

  if (key == KEY_ARROW_RIGHT) {
    if (cursor_position < input_length) {
      cursor_position++;
      terminal_set_cursor(SHELL_PROMPT_COLUMN + cursor_position);
    }
    return;
  }

  if (key == KEY_HOME) {
    cursor_position = 0;
    terminal_set_cursor(SHELL_PROMPT_COLUMN + cursor_position);
    return;
  }

  if (key == KEY_END) {
    cursor_position = input_length;
    terminal_set_cursor(SHELL_PROMPT_COLUMN + cursor_position);
    return;
  }
}
