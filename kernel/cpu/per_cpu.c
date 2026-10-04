#include "per_cpu.h"

static cpu_t boot_cpu;

void cpu_init(void) {
  boot_cpu.id = 0;

  boot_cpu.current_thread = 0;

  boot_cpu.ready_head = 0;
  boot_cpu.ready_tail = 0;

  boot_cpu.idle_thread = 0;

  boot_cpu.interrupt_next_thread = 0;
  boot_cpu.interrupt_next_rsp = 0;
  boot_cpu.interrupt_next_cr3 = 0;

  boot_cpu.interrupt_next_is_preempted = 0;
  boot_cpu.interrupt_switch_required = 0;
}

cpu_t *cpu_current(void) { return &boot_cpu; }

struct thread *cpu_current_thread(void) {
  return cpu_current()->current_thread;
}

void cpu_set_current_thread(struct thread *thread) {
  cpu_current()->current_thread = thread;
}
