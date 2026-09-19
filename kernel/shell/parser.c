#include "parser.h"
#include <stddef.h>

int shell_parse_command(char *input, int *argc, char *argv[]) {
  *argc = 0;

  while (*input) {
    while (*input == ' ') {
      input++;
    }

    if (!*input)
      break;

    if (*argc >= SHELL_MAX_ARGUMENTS) {
      return 0;
    }

    argv[*argc] = input;
    (*argc)++;

    while (*input && *input != ' ') {
      input++;
    }

    if (*input) {
      *input = '\0';
      input++;
    }
  }
  argv[*argc] = NULL;
  return 1;
}
