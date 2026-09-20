#include "shell.h"
#include "commands/commands.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/terminal/terminal.h"
#include "history.h"
#include "lib/string.h"
#include "parser.h"
#include "shell_config.h"

static char input_buffer[SHELL_BUFFER_SIZE];
static unsigned int input_length = 0;
static unsigned int cursor_position = 0;
static unsigned int shell_start_row = 0;

void shell_init(void) {
  input_length = 0;
  cursor_position = 0;
  shell_start_row = terminal_get_row();
  history_init();
}

void shell_print_prompt(void) { terminal_write(SHELL_PROMPT); }

static void shell_update_cursor(void) {
  unsigned int absolute_position = SHELL_PROMPT_COLUMN + cursor_position;

  unsigned int row_offset = absolute_position / TERMINAL_WIDTH;

  unsigned int column = absolute_position % TERMINAL_WIDTH;

  unsigned int target_row = shell_start_row + row_offset;

  unsigned int current_row = terminal_get_row();

  int row_difference = (int)target_row - (int)current_row;

  terminal_move_cursor_row(row_difference);
  terminal_set_cursor(column);
}

static void shell_redraw_input(unsigned int old_length) {
  unsigned int max_length =
      old_length > input_length ? old_length : input_length;

  /*
   * Make sure the cursor position has room on screen.
   *
   * The cursor itself can be on the next row when the
   * input exactly reaches the end of a line.
   */
  unsigned int cursor_absolute = SHELL_PROMPT_COLUMN + input_length;

  unsigned int cursor_row_offset = cursor_absolute / TERMINAL_WIDTH;

  while (shell_start_row + cursor_row_offset >= TERMINAL_HEIGHT) {
    terminal_scroll();
    shell_start_row--;
  }

  /*
   * Clear old input.
   */
  for (unsigned int i = 0; i < max_length; i++) {
    unsigned int absolute = SHELL_PROMPT_COLUMN + i;

    unsigned int row = shell_start_row + absolute / TERMINAL_WIDTH;

    unsigned int column = absolute % TERMINAL_WIDTH;

    terminal_put_at(row, column, ' ');
  }

  /*
   * Draw new input.
   */
  for (unsigned int i = 0; i < input_length; i++) {
    unsigned int absolute = SHELL_PROMPT_COLUMN + i;

    unsigned int row = shell_start_row + absolute / TERMINAL_WIDTH;

    unsigned int column = absolute % TERMINAL_WIDTH;

    terminal_put_at(row, column, input_buffer[i]);
  }

  /*
   * Put the hardware cursor at the logical
   * shell cursor position.
   */
  shell_update_cursor();
}

static void shell_load_history_command(const char *command) {
  unsigned int old_length = input_length;
  string_copy(input_buffer, command);
  input_length = 0;
  while (input_buffer[input_length] != '\0') {
    input_length++;
  }
  cursor_position = input_length;
  shell_redraw_input(old_length);
}

static void shell_process_command(void) {
  char *argv[SHELL_MAX_ARGUMENTS + 1];
  int argc;

  shell_parse_result result = shell_parse_command(input_buffer, &argc, argv);

  if (result == SHELL_PARSE_TOO_MANY_ARGUMENTS) {
    terminal_write("Too many arguments\n");
    return;
  }

  if (result == SHELL_PARSE_UNCLOSED_QUOTE) {
    terminal_write("Unclosed quote\n");
    return;
  }

  if (argc == 0)
    return;

  shell_execute_command(argc, argv);
}

static void shell_clear_input(void) {
  unsigned int old_length = input_length;

  input_length = 0;
  cursor_position = 0;
  shell_redraw_input(old_length);
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
      history_reset_position();
      shell_redraw_input(old_length);
    }
    return;
  }

  if (key == KEY_CTRL_C) {
    shell_clear_input();
    terminal_putchar('\n');
    shell_print_prompt();
    shell_start_row = terminal_get_row();
    return;
  }

  if (key == KEY_CTRL_L) {
    terminal_clear();
    shell_print_prompt();
    shell_start_row = terminal_get_row();
    return;
  }

  if (key == KEY_BACKSPACE) {
    unsigned int old_length = input_length;
    if (cursor_position > 0) {
      for (unsigned int i = cursor_position - 1; i < input_length - 1; i++) {
        input_buffer[i] = input_buffer[i + 1];
      }

      input_length--;
      cursor_position--;
      history_reset_position();
      shell_redraw_input(old_length);
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
      history_reset_position();
      shell_redraw_input(old_length);
    }

    return;
  }

  if (key == KEY_ENTER) {
    input_buffer[input_length] = '\0';
    history_add(input_buffer);
    cursor_position = input_length;

    shell_update_cursor();

    unsigned int absolute_position = SHELL_PROMPT_COLUMN + input_length;

    unsigned int cursor_column = absolute_position % TERMINAL_WIDTH;

    /*
     * If the cursor is already at column 0 on the
     * next line, the terminal has effectively wrapped.
     * Do not add another newline.
     */
    if (cursor_column != 0) {
      terminal_putchar('\n');
    }

    shell_process_command();

    input_length = 0;
    cursor_position = 0;

    shell_print_prompt();
    shell_start_row = terminal_get_row();

    return;
  }
  if (key == KEY_ARROW_LEFT) {
    if (cursor_position > 0) {
      cursor_position--;
      shell_update_cursor();
    }
    return;
  }

  if (key == KEY_ARROW_RIGHT) {
    if (cursor_position < input_length) {
      cursor_position++;
      shell_update_cursor();
    }
    return;
  }

  if (key == KEY_HOME) {
    cursor_position = 0;
    shell_update_cursor();
    return;
  }

  if (key == KEY_END) {
    cursor_position = input_length;
    shell_update_cursor();
    return;
  }

  if (key == KEY_ARROW_UP) {
    const char *command = history_previous();
    if (command != 0) {
      shell_load_history_command(command);
    }
    return;
  }

  if (key == KEY_ARROW_DOWN) {
    const char *command = history_next();
    if (command != 0) {
      shell_load_history_command(command);
    } else {
      shell_clear_input();
    }
    return;
  }
}
