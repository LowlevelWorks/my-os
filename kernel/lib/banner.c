#include "banner.h"
#include "drivers/terminal/terminal.h"

static const char *banner[] = {
    "'##::::'##:'##:::'##:::::'#######:::'######::",
    " ###::'###:. ##:'##:::::'##.... ##:'##... ##:",
    " ####'####::. ####:::::: ##:::: ##: ##:::..::",
    " ## ### ##:::. ##::::::: ##:::: ##:. ######::",
    " ##. #: ##:::: ##::::::: ##:::: ##::..... ##:",
    " ##:.:: ##:::: ##::::::: ##:::: ##:'##::: ##:",
    " ##:::: ##:::: ##:::::::. #######::. ######::",
    "..:::::..:::::..:::::..:::::.......::::......::",
    "",
    "  x86_64 Kernel | Custom Operating System",
    "",
    "  Type 'help' to get started.",
    "",
    0,
};

void kernel_print_banner(void) {
  for (unsigned int i = 0; banner[i] != 0; i++) {
    terminal_write(banner[i]);
    terminal_putchar('\n');
  }
}
