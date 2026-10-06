#include "scheduler.h"

#include "arch/x86_64/context_switch.h"
#include "arch/x86_64/cpu.h"
#include "cpu/per_cpu.h"

#include "memory/address_space/address_space.h"
#include "memory/paging/paging.h"
#include "panic/panic.h"

#include "process/process/process.h"
#include "process/thread/thread.h"

static uint64_t scheduler_thread_cr3(thread_t *thread) {
  if (thread == 0)
    panic("scheduler_thread_cr3: null thread");

  if (thread->process == 0)
    return cpu_read_cr3() & PAGE_ADDRESS_MASK;

  if (thread->process->address_space == 0)
    panic("scheduler_thread_cr3: process has no address space");

  return thread->process->address_space->page_table_root;
}

static void scheduler_clear_interrupt_switch(cpu_t *cpu) {
  cpu->interrupt_next_thread = 0;
  cpu->interrupt_next_rsp = 0;
  cpu->interrupt_next_cr3 = 0;
  cpu->interrupt_next_is_preempted = 0;
  cpu->interrupt_switch_required = 0;
}

static void scheduler_validate_next(thread_t *thread) {
  if (thread == 0)
    panic("scheduler_validate_next: null thread");

  if (thread->state == THREAD_TERMINATED)
    panic("scheduler_validate_next: terminated thread");

  if (thread->state == THREAD_BLOCKED)
    panic("scheduler_validate_next: blocked thread");
}

void scheduler_init(void) {
  cpu_t *cpu = cpu_current();

  cpu->ready_head = 0;
  cpu->ready_tail = 0;

  scheduler_clear_interrupt_switch(cpu);
}

void scheduler_add(thread_t *thread) {
  if (thread == 0)
    panic("scheduler_add: null thread");

  if (thread->state == THREAD_TERMINATED)
    panic("scheduler_add: terminated thread");

  cpu_t *cpu = cpu_current();

  if (thread == cpu->idle_thread)
    return;

  thread->next = 0;
  thread->state = THREAD_READY;

  if (cpu->ready_tail == 0) {
    cpu->ready_head = thread;
    cpu->ready_tail = thread;
    return;
  }

  cpu->ready_tail->next = thread;
  cpu->ready_tail = thread;
}

thread_t *scheduler_next(void) {
  cpu_t *cpu = cpu_current();
  thread_t *thread = cpu->ready_head;

  if (thread == 0)
    return cpu->idle_thread;

  cpu->ready_head = thread->next;
  thread->next = 0;

  if (cpu->ready_head == 0)
    cpu->ready_tail = 0;

  return thread;
}

void scheduler_block_current(void) {
  __asm__ volatile("cli");

  cpu_t *cpu = cpu_current();
  thread_t *current = cpu->current_thread;

  if (current == 0)
    panic("scheduler_block_current: no current thread");

  if (current == cpu->idle_thread)
    panic("scheduler_block_current: cannot block idle thread");

  if (current->state != THREAD_BLOCKED)
    panic("scheduler_block_current: thread is not blocked");

  current->interrupt_rsp = 0;

  thread_t *next = scheduler_next();

  scheduler_validate_next(next);

  next->state = THREAD_RUNNING;

  uint64_t new_cr3 = scheduler_thread_cr3(next);

  thread_set_current(next);

  if (next->interrupt_rsp != 0) {
    uint64_t frame = next->interrupt_rsp;
    next->interrupt_rsp = 0;

    context_switch_to_preempted(&current->saved_rsp, frame, new_cr3);
  }

  context_switch(&current->saved_rsp, next->saved_rsp, new_cr3);
}

void scheduler_wake(thread_t *thread) {
  if (thread == 0)
    panic("scheduler_wake: null thread");

  if (thread->state != THREAD_BLOCKED)
    panic("scheduler_wake: thread is not blocked");

  scheduler_add(thread);
}

void scheduler_start(void) {
  thread_t *next = scheduler_next();

  scheduler_validate_next(next);

  next->state = THREAD_RUNNING;
  next->interrupt_rsp = 0;

  thread_set_current(next);

  context_switch(0, next->saved_rsp, scheduler_thread_cr3(next));

  panic("scheduler_start: context_switch returned");
}

void scheduler_create_idle(void) {
  cpu_t *cpu = cpu_current();

  if (cpu->idle_thread != 0)
    panic("scheduler_create_idle: idle thread already exists");

  cpu->idle_thread = thread_create_idle();
}

void scheduler_tick(struct interrupt_frame *frame) {
  if (frame == 0)
    panic("scheduler_tick: null interrupt frame");

  cpu_t *cpu = cpu_current();

  scheduler_clear_interrupt_switch(cpu);

  thread_t *current = cpu->current_thread;

  if (current == 0)
    return;

  if (current->state == THREAD_TERMINATED)
    return;

  current->interrupt_rsp = (uint64_t)frame;

  if (current != cpu->idle_thread && current->state == THREAD_RUNNING) {
    scheduler_add(current);
  }

  thread_t *next = scheduler_next();

  scheduler_validate_next(next);

  if (next == current) {
    current->state = THREAD_RUNNING;
    current->interrupt_rsp = 0;
    return;
  }

  next->state = THREAD_RUNNING;

  cpu->interrupt_next_thread = next;

  if (next->interrupt_rsp != 0) {
    cpu->interrupt_next_rsp = next->interrupt_rsp;
    cpu->interrupt_next_is_preempted = 1;
    next->interrupt_rsp = 0;
  } else {
    cpu->interrupt_next_rsp = next->saved_rsp;
    cpu->interrupt_next_is_preempted = 0;
  }

  cpu->interrupt_next_cr3 = scheduler_thread_cr3(next);

  thread_set_current(next);

  cpu->interrupt_switch_required = 1;
}

int scheduler_interrupt_switch_required(void) {
  return cpu_current()->interrupt_switch_required;
}

uint64_t scheduler_interrupt_next_rsp(void) {
  cpu_t *cpu = cpu_current();

  if (!cpu->interrupt_switch_required)
    panic("scheduler_interrupt_next_rsp: no switch");

  if (cpu->interrupt_next_rsp == 0)
    panic("scheduler_interrupt_next_rsp: invalid stack");

  return cpu->interrupt_next_rsp;
}

uint64_t scheduler_interrupt_next_cr3(void) {
  cpu_t *cpu = cpu_current();

  if (!cpu->interrupt_switch_required)
    panic("scheduler_interrupt_next_cr3: no switch");

  if (cpu->interrupt_next_cr3 == 0)
    panic("scheduler_interrupt_next_cr3: invalid CR3");

  return cpu->interrupt_next_cr3;
}

int scheduler_interrupt_next_is_preempted(void) {
  cpu_t *cpu = cpu_current();

  if (!cpu->interrupt_switch_required)
    panic("scheduler_interrupt_next_is_preempted: no switch");

  return cpu->interrupt_next_is_preempted;
}
