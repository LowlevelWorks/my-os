#include "clear.h"

#include "drivers/terminal/terminal.h"

void clear_command(int argc, char *argv[]) {
  (void)argv;
  if (argc != 1) {
    terminal_write("Usage: clear\n");
    return;
  }

  terminal_clear();
}
