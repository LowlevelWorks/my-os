#include "echo.h"

#include "drivers/terminal/terminal.h"

void echo_command(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    terminal_write(argv[i]);

    if (i + 1 < argc) {
      terminal_putchar(' ');
    }
  }

  terminal_putchar('\n');
}
