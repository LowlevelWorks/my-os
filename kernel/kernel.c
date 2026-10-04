#include <stdint.h>

#include "arch/x86_64/cpu.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/multiboot.h"
#include "arch/x86_64/pic.h"
#include "cpu/per_cpu.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/terminal/terminal.h"
#include "memory/address_space/address_space.h"
#include "memory/buddy/buddy.h"
#include "memory/frame/frame.h"
#include "memory/paging/paging.h"
#include "memory/physical_memory/physical_memory.h"
#include "panic/panic.h"
#include "pit.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "process/thread.h"
#include "shell/shell.h"

static void process1_entry(void) {
  terminal_write("PROCESS 1: running\n");

  for (volatile uint64_t i = 0; i < 10000000; i++) {
  }

  terminal_write("PROCESS 1: exiting\n");
}

static void process2_entry(void) {
  terminal_write("PROCESS 2: running\n");

  for (volatile uint64_t i = 0; i < 10000000; i++) {
  }

  terminal_write("PROCESS 2: exiting\n");
}

void kernel_main(multiboot_info_t *multiboot_info) {
  terminal_init();

  physical_memory_init(multiboot_info);
  buddy_init();
  paging_init();
  gdt_init();

  idt_init();
  pic_initialize();
  pic_unmask_irq(0);
  pic_unmask_irq(1);

  pit_init(100);

  shell_init();
  shell_print_prompt();

  cpu_init();
  process_init();
  thread_init();
  scheduler_init();
  scheduler_create_idle();

  terminal_write("\n");
  terminal_write("=== PROCESS / ADDRESS SPACE TEST ===\n");

  process_t *process1 = process_create_kernel();
  process_t *process2 = process_create_kernel();

  if (process1 == 0 || process2 == 0) {
    panic("process test: process creation failed");
  }

  terminal_write("Process 1 created: PID ");
  terminal_write_uint(process1->id);
  terminal_write("\n");

  terminal_write("Process 2 created: PID ");
  terminal_write_uint(process2->id);
  terminal_write("\n");

  thread_t *thread1 = process_create_thread(process1, process1_entry);

  thread_t *thread2 = process_create_thread(process2, process2_entry);

  if (thread1 == 0 || thread2 == 0) {
    panic("process test: thread creation failed");
  }

  physical_frame_t frame1 = frame_alloc();
  physical_frame_t frame2 = frame_alloc();

  if (frame1 == INVALID_FRAME || frame2 == INVALID_FRAME) {
    panic("address-space test: unable to allocate test frames");
  }

  uint64_t physical1 = frame_to_address(frame1);

  uint64_t physical2 = frame_to_address(frame2);

  uint64_t test_virtual_address = 0x400000;

  paging_result_t result =
      address_space_map(process1->address_space, test_virtual_address,
                        physical1, PAGE_WRITABLE | PAGE_USER);

  if (result != PAGING_OK) {
    panic("address-space test: process 1 mapping failed");
  }

  result = address_space_map(process2->address_space, test_virtual_address,
                             physical2, PAGE_WRITABLE | PAGE_USER);

  if (result != PAGING_OK) {
    panic("address-space test: process 2 mapping failed");
  }

  terminal_write("Process 1: VA 0x400000 -> PA 0x");
  terminal_write_uint(physical1);
  terminal_write("\n");

  terminal_write("Process 2: VA 0x400000 -> PA 0x");
  terminal_write_uint(physical2);
  terminal_write("\n");

  if (physical1 == physical2) {
    panic("address-space test: test frames are identical");
  }

  scheduler_add(thread1);
  scheduler_add(thread2);

  scheduler_start();

  __asm__ volatile("sti");

  panic("kernel_main: scheduler_start returned");
}
