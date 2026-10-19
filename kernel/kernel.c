#include <stdint.h>

#include "arch/x86_64/gdt.h"
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/multiboot.h"
#include "arch/x86_64/pic.h"
#include "cpu/per_cpu.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/serial/serial.h"
#include "drivers/terminal/terminal.h"
#include "lib/banner.h"
#include "memory/buddy/buddy.h"
#include "memory/paging/paging.h"
#include "memory/physical_memory/physical_memory.h"
#include "panic/panic.h"
#include "pit.h"
#include "process/process/process.h"
#include "process/scheduler/scheduler.h"
#include "process/sleep/sleep.h"
#include "process/thread/thread.h"
#include "shell/shell.h"

static void reaper_entry(void) {
  process_t *reaper = process_get_reaper();

  for (;;) {
    int status;
    int child_id = process_wait(reaper, &status);

    if (child_id < 0)
      thread_sleep(1);
  }
}

static void shell_entry(void) {
  for (;;) {
    if (keyboard_has_input()) {
      uint16_t key = keyboard_getkey();
      shell_handle_key(key);
      continue;
    }

    shell_poll_serial();
  }
}

void kernel_main(multiboot_info_t *multiboot_info) {
  terminal_init();
  serial_init();

  physical_memory_init(multiboot_info);
  buddy_init();
  paging_init();
  gdt_init();

  idt_init();
  pic_initialize();
  pic_unmask_irq(0);
  pic_unmask_irq(1);

  pit_init(100);
  sleep_init();

  kernel_print_banner();
  shell_init();
  shell_print_prompt();

  cpu_init();
  process_init();
  thread_init();
  scheduler_init();
  scheduler_create_idle();
  process_init_reaper();

  thread_t *reaper_thread =
      process_create_thread(process_get_reaper(), reaper_entry);

  if (reaper_thread == 0)
    panic("kernel_main: unable to create reaper thread");

  process_start(process_get_reaper());

  process_t *shell_process = process_create(process_get_reaper());

  if (shell_process == 0)
    panic("kernel_main: unable to create shell process");

  thread_t *shell_thread = process_create_thread(shell_process, shell_entry);

  if (shell_thread == 0)
    panic("kernel_main: unable to create shell thread");

  process_start(shell_process);

  scheduler_start();

  for (;;) {
    __asm__ volatile("hlt");
  }
}
