#include "panic.h"
#include "drivers/terminal/terminal.h"

void panic(const char *message) {
  terminal_write("PANIC: ");
  terminal_write(message);
  terminal_write("\n");

  __asm__ volatile("cli");

  for (;;) {
    __asm__ volatile("hlt");
  }
}
