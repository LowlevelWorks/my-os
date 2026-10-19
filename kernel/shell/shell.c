#include "shell.h"
#include "commands/commands.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/serial/serial.h"
#include "drivers/terminal/terminal.h"
#include "history.h"
#include "lib/string.h"
#include "parser.h"
#include "shell_config.h"

static char input_buffer[SHELL_BUFFER_SIZE];
static unsigned int input_length = 0;
static unsigned int cursor_position = 0;
static unsigned int shell_start_row = 0;

/* ---- Serial mirror ------------------------------------------------------
 *
 * VGA is drawn by absolute position (terminal_put_at). A serial terminal has
 * no absolute positioning, so the input line is mirrored with a stateless
 * repaint: CR to column 0, skip over the prompt, erase to end of line,
 * write the buffer, then place the cursor.
 *
 * Limitation: this assumes the input fits on one terminal line
 * (prompt + input shorter than the host terminal width).
 */

static void shell_serial_csi(unsigned int n, char cmd) {
  char digits[12];
  int i = 0;

  if (n == 0) {
    return; /* "move 0" is interpreted as "move 1" by terminals */
  }

  serial_putc('\x1b');
  serial_putc('[');
  while (n) {
    digits[i++] = '0' + (n % 10);
    n /= 10;
  }
  while (i) {
    serial_putc(digits[--i]);
  }
  serial_putc(cmd);
}

static void shell_serial_sync(void) {
  serial_putc('\r');
  shell_serial_csi(SHELL_PROMPT_COLUMN, 'C');

  /* Erase from the cursor to the end of the line. */
  serial_putc('\x1b');
  serial_putc('[');
  serial_putc('K');

  for (unsigned int i = 0; i < input_length; i++) {
    serial_putc(input_buffer[i]);
  }

  serial_putc('\r');
  shell_serial_csi(SHELL_PROMPT_COLUMN + cursor_position, 'C');
}

/* ------------------------------------------------------------------------ */

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

  /* Mirror the same state to the serial console. */
  shell_serial_sync();
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

static void shell_submit(void) {
  input_buffer[input_length] = '\0';
  history_add(input_buffer);
  cursor_position = input_length;

  shell_update_cursor();
  shell_serial_sync(); /* host cursor to the end of the input */

  unsigned int absolute_position = SHELL_PROMPT_COLUMN + input_length;

  unsigned int cursor_column = absolute_position % TERMINAL_WIDTH;

  /*
   * If the cursor is already at column 0 on the
   * next line, VGA has effectively wrapped and needs no
   * newline. The serial terminal still needs one.
   */
  if (cursor_column != 0) {
    terminal_putchar('\n');
  } else {
    serial_putc('\n');
  }

  shell_process_command();

  input_length = 0;
  cursor_position = 0;

  shell_print_prompt();
  shell_start_row = terminal_get_row();
}

static void shell_backspace(void) {
  if (cursor_position == 0) {
    return;
  }

  unsigned int old_length = input_length;

  for (unsigned int i = cursor_position - 1; i < input_length - 1; i++) {
    input_buffer[i] = input_buffer[i + 1];
  }

  input_length--;
  cursor_position--;
  history_reset_position();

  /*
   * Backspace arrives as DEL (0x7F) or CR (0x08); move the
   * cursor back over the last character, erase it, and leave
   * the cursor where the next key press will land.
   */
  terminal_putchar('\b');
  terminal_putchar(' ');
  terminal_putchar('\b');

  /* The redraw clears the old text and mirrors the change to serial. */
  shell_redraw_input(old_length);
}

static void shell_clear_input(void) {
  unsigned int old_length = input_length;

  input_length = 0;
  cursor_position = 0;
  shell_redraw_input(old_length);
}

void shell_handle_key(uint16_t key) {
  /*
   * Enter arrives as '\n' (serial CR is converted by the driver)
   * and backspace as '\b' or 0x7F (DEL).
   */
  if (key == '\n' || key == '\r') {
    shell_submit();
    return;
  }

  if (key == '\b' || key == '\x7F') {
    shell_backspace();
    return;
  }

  /* Printable ASCII only; control bytes must not end up in the buffer. */
  if (key >= 0x20 && key < 0x7F) {
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
    shell_backspace();
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
    shell_submit();
    return;
  }
  if (key == KEY_ARROW_LEFT) {
    if (cursor_position > 0) {
      cursor_position--;
      shell_update_cursor();
      shell_serial_sync();
    }
    return;
  }

  if (key == KEY_ARROW_RIGHT) {
    if (cursor_position < input_length) {
      cursor_position++;
      shell_update_cursor();
      shell_serial_sync();
    }
    return;
  }

  if (key == KEY_HOME) {
    cursor_position = 0;
    shell_update_cursor();
    shell_serial_sync();
    return;
  }

  if (key == KEY_END) {
    cursor_position = input_length;
    shell_update_cursor();
    shell_serial_sync();
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

/* ---- Serial input ---------------------------------------------------------
 *
 * Translates raw serial bytes (including xterm escape sequences for the
 * arrow/home/end/delete keys) into the same key codes the keyboard driver
 * produces, then feeds them to shell_handle_key().
 *
 * Call shell_poll_serial() from the same loop that polls the keyboard.
 */

static int esc_state = 0; /* 0 = normal, 1 = after ESC, 2 = inside CSI */
static unsigned int esc_param = 0;

static uint16_t serial_byte_to_key(uint8_t b) {
  if (esc_state == 0) {
    if (b == 0x1B) {
      esc_state = 1;
      return 0;
    }
    if (b == 0x03) {
      return KEY_CTRL_C;
    }
    if (b == 0x0C) {
      return KEY_CTRL_L;
    }
    if (b == '\n' || b == '\b' || b == 0x7F) {
      return b;
    }
    if (b >= 0x20 && b < 0x7F) {
      return b;
    }
    return 0; /* ignore other control bytes (tab, etc.) */
  }

  if (esc_state == 1) {
    if (b == '[' || b == 'O') {
      esc_state = 2;
      esc_param = 0;
    } else {
      esc_state = 0;
    }
    return 0;
  }

  /* esc_state == 2: collecting parameters until a final byte */
  if (b >= '0' && b <= '9') {
    esc_param = esc_param * 10 + (b - '0');
    return 0;
  }
  if (b == ';') {
    return 0;
  }

  esc_state = 0;

  switch (b) {
  case 'A':
    return KEY_ARROW_UP;
  case 'B':
    return KEY_ARROW_DOWN;
  case 'C':
    return KEY_ARROW_RIGHT;
  case 'D':
    return KEY_ARROW_LEFT;
  case 'H':
    return KEY_HOME;
  case 'F':
    return KEY_END;
  case '~':
    if (esc_param == 3)
      return KEY_DELETE;
    if (esc_param == 1 || esc_param == 7)
      return KEY_HOME;
    if (esc_param == 4 || esc_param == 8)
      return KEY_END;
    return 0;
  default:
    return 0;
  }
}

void shell_poll_serial(void) {
  uint8_t b;

  while (serial_try_getc(&b)) {
    uint16_t key = serial_byte_to_key(b);
    if (key != 0) {
      shell_handle_key(key);
    }
  }
}
