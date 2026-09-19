#ifndef SHELL_COMMANDS_H
#define SHELL_COMMANDS_H

typedef void (*shell_command_handler)(int argc, char *argv[]);

typedef struct {
  const char *name;
  shell_command_handler handler;
} shell_command;

int shell_execute_command(int argc, char *argv[]);

#endif
