#include "parser.h"
#include <stddef.h>

static int is_whitespace(char c) { return c == ' ' || c == '\t'; }

shell_parse_result shell_parse_command(char *input, int *argc, char *argv[]) {
  *argc = 0;

  while (*input) {
    while (is_whitespace(*input)) {
      input++;
    }

    if (!*input)
      break;

    if (*argc >= SHELL_MAX_ARGUMENTS) {
      return SHELL_PARSE_TOO_MANY_ARGUMENTS;
    }

    argv[*argc] = input;
    (*argc)++;

    char quote = 0;
    char *output = input;

    while (*input) {
      if (quote) {
        if (*input == '\\') {
          input++;

          if (!*input)
            return SHELL_PARSE_UNCLOSED_QUOTE;

          *output = *input;
          output++;
          input++;
          continue;
        }

        if (*input == quote) {
          quote = 0;
          input++;
          continue;
        }
        *output = *input;
        output++;
        input++;
        continue;
      }
      if (*input == '\'' || *input == '"') {
        quote = *input;
        input++;
        continue;
      }

      if (is_whitespace(*input)) {
        break;
      }
      *output = *input;
      output++;
      input++;
    }

    if (quote) {
      return SHELL_PARSE_UNCLOSED_QUOTE;
    }

    char *next = *input ? input + 1 : input;
    *output = '\0';
    input = next;
  }

  argv[*argc] = NULL;
  return SHELL_PARSE_SUCCESS;
}
