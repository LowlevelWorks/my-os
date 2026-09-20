#include "lib/string.h"
#include "shell_config.h"

static char command_history[SHELL_HISTORY_SIZE][SHELL_BUFFER_SIZE];

static unsigned int history_start = 0;
static unsigned int history_count = 0;
static int history_position = -1;

void history_init(void) {
  history_start = 0;
  history_count = 0;
  history_position = -1;
}

void history_reset_position(void) { history_position = -1; }

static unsigned int history_index(unsigned int position) {
  return (history_start + position) % SHELL_HISTORY_SIZE;
}

void history_add(const char *command) {
  if (command[0] == '\0')
    return;
  unsigned int index;

  if (history_count < SHELL_HISTORY_SIZE) {
    index = history_index(history_count);
    history_count++;
  } else {
    index = history_start;
    history_start = (history_start + 1) % SHELL_HISTORY_SIZE;
  }

  string_copy(command_history[index], command);
  history_position = -1;
}

const char *history_previous(void) {
  if (history_count == 0)
    return 0;

  if (history_position == -1) {
    history_position = (int)history_count - 1;
  } else if (history_position > 0) {
    history_position--;
  }
  return command_history[history_index((unsigned int)history_position)];
}

const char *history_next(void) {
  if (history_position == -1)
    return 0;

  if (history_position < (int)history_count - 1) {
    history_position++;
    return command_history[history_index((unsigned int)history_position)];
  }
  history_position = -1;
  return 0;
}
