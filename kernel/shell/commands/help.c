#include "help.h"

#include "drivers/terminal/terminal.h"

void help_command(int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    terminal_write("Usage: help\n");
    return;
  }

  terminal_write("Available commands:\n");
  terminal_write("help\n");
  terminal_write("echo\n");
  terminal_write("clear\n");
}
