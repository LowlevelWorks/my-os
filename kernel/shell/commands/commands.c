#include "commands.h"

#include "clear.h"
#include "echo.h"
#include "help.h"

#include "drivers/terminal/terminal.h"
#include "lib/string.h"

static const shell_command commands[] = {
    {"help", help_command},
    {"echo", echo_command},
    {"clear", clear_command},
};

int shell_execute_command(int argc, char *argv[]) {
  for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {

    if (string_equals(argv[0], commands[i].name)) {
      commands[i].handler(argc, argv);
      return 1;
    }
  }

  terminal_write("Unknown command: ");
  terminal_write(argv[0]);
  terminal_putchar('\n');

  return 0;
}
