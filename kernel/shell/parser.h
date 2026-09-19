#ifndef SHELL_PARSER_H
#define SHELL_PARSER_H

#define SHELL_MAX_ARGUMENTS 16

int shell_parse_command(char *input, int *argc, char *argv[]);

#endif
