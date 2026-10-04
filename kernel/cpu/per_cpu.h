#ifndef PER_CPU_H
#define PER_CPU_H

#include <stdint.h>

struct thread;

typedef struct cpu {
  uint32_t id;

  struct thread *current_thread;

  struct thread *ready_head;
  struct thread *ready_tail;

  struct thread *idle_thread;

  struct thread *interrupt_next_thread;

  uint64_t interrupt_next_rsp;
  uint64_t interrupt_next_cr3;

  int interrupt_next_is_preempted;
  int interrupt_switch_required;
} cpu_t;

void cpu_init(void);

cpu_t *cpu_current(void);

struct thread *cpu_current_thread(void);

void cpu_set_current_thread(struct thread *thread);

#endif
