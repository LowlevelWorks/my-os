#include <stdint.h>

#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/multiboot.h"
#include "arch/x86_64/pic.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/terminal/terminal.h"
#include "memory/paging.h"
#include "memory/physical_memory.h"
#include "shell/shell.h"

static void enable_interrupts(void) { __asm__ volatile("sti"); }

void kernel_main(multiboot_info_t *multiboot_info) {
  terminal_init();

  physical_memory_init(multiboot_info);
  terminal_write("regions: ");
  terminal_write_uint(physical_memory_region_count());
  terminal_write("\nhighest: ");
  terminal_write_uint(physical_memory_highest_address());
  terminal_write("\nmb flags: ");
  terminal_write_uint(multiboot_info ? multiboot_info->flags : 0xDEAD);
  terminal_write("\n");
  paging_init();

  if (physical_memory_init_frame_allocator() != 0) {
    terminal_write("kkkkk");
    for (;;) {
      __asm__ volatile("cli");
      __asm__ volatile("hlt");
    }
  }

  physical_memory_print();

  idt_init();

  pic_initialize();
  pic_unmask_irq(1);

  shell_init();
  shell_print_prompt();
  enable_interrupts();

  while (1) {
    if (keyboard_has_input()) {
      uint16_t key = keyboard_getkey();

      shell_handle_key(key);
    }

    __asm__ volatile("hlt");
  }
}
