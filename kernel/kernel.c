#include <stdint.h>

#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/pic.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/terminal/terminal.h"
#include "shell/shell.h"

static void enable_interrupts(void) { __asm__ volatile("sti"); }

void kernel_main(void) {
  terminal_init();

  idt_init();

  pic_initialize();
  pic_unmask_irq(1);

  shell_init();

  terminal_write("> ");

  enable_interrupts();

  while (1) {
    if (keyboard_has_input()) {
      uint16_t key = keyboard_getkey();

      shell_handle_key(key);
    }

    __asm__ volatile("hlt");
  }
}
