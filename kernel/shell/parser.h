#ifndef SHELL_PARSER_H
#define SHELL_PARSER_H

#define SHELL_MAX_ARGUMENTS 16

typedef enum {
  SHELL_PARSE_SUCCESS,
  SHELL_PARSE_TOO_MANY_ARGUMENTS,
  SHELL_PARSE_UNCLOSED_QUOTE
} shell_parse_result;

shell_parse_result shell_parse_command(char *input, int *argc, char *argv[]);

#endif
