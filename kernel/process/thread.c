#include "thread.h"

#include "arch/x86_64/context_switch.h"
#include "arch/x86_64/cpu.h"
#include "cpu/per_cpu.h"

#include "memory/address_space/address_space.h"
#include "memory/buddy/buddy.h"
#include "memory/frame/frame.h"
#include "memory/paging/paging.h"
#include "panic/panic.h"

#include "lib/string.h"
#include "process/process.h"
#include "process/scheduler.h"

#define THREAD_STACK_ORDER 2
#define THREAD_STACK_FRAMES (1ULL << THREAD_STACK_ORDER)
#define THREAD_STACK_SIZE (THREAD_STACK_FRAMES * PAGE_SIZE)

static thread_id_t next_thread_id = 1;

thread_t *thread_current(void) { return cpu_current_thread(); }

void thread_set_current(thread_t *thread) { cpu_set_current_thread(thread); }

static void thread_trampoline(void) {
  __asm__ volatile("sti");

  thread_t *current = thread_current();

  if (current == 0) {
    panic("thread_trampoline: no current thread");
  }

  if (current->entry == 0) {
    panic("thread_trampoline: no entry");
  }

  current->entry();

  thread_exit();

  panic("thread_trampoline: thread_exit returned");
}

void thread_init(void) {
  next_thread_id = 1;
  thread_set_current(0);
}

thread_t *thread_create_kernel_for_process(struct process *process,
                                           void (*entry)(void)) {
  if (entry == 0) {
    panic("thread_create_kernel_for_process: null entry");
  }

  if (process != 0 && process->address_space == 0) {
    panic("thread_create_kernel_for_process: process has no address space");
  }

  physical_frame_t thread_frame = frame_alloc();

  if (thread_frame == INVALID_FRAME) {
    panic("thread_create_kernel_for_process: unable to allocate thread");
  }

  thread_t *thread =
      (thread_t *)paging_physical_to_virtual(frame_to_address(thread_frame));

  memory_set(thread, 0, sizeof(thread_t));

  physical_frame_t stack_frame = buddy_alloc(THREAD_STACK_ORDER);

  if (stack_frame == INVALID_FRAME) {
    frame_free(thread_frame);
    panic("thread_create_kernel_for_process: unable to allocate kernel stack");
  }

  uint64_t stack_base =
      paging_physical_to_virtual(frame_to_address(stack_frame));

  uint64_t stack_top = stack_base + THREAD_STACK_SIZE;

  uint64_t *stack = (uint64_t *)stack_top;

  *--stack = 0;
  *--stack = (uint64_t)thread_trampoline;
  *--stack = 0;
  *--stack = 0;
  *--stack = 0;
  *--stack = 0;
  *--stack = 0;
  *--stack = 0;

  thread->id = next_thread_id++;
  thread->state = THREAD_READY;
  thread->kernel_stack_base = stack_base;
  thread->kernel_stack_top = stack_top;
  thread->saved_rsp = (uint64_t)stack;
  thread->interrupt_rsp = 0;
  thread->entry = entry;
  thread->process = process;
  thread->next = 0;
  thread->process_next = 0;

  return thread;
}

thread_t *thread_create_kernel(void (*entry)(void)) {
  return thread_create_kernel_for_process(0, entry);
}

static void thread_idle(void) {
  for (;;) {
    __asm__ volatile("hlt");
  }
}

thread_t *thread_create_idle(void) { return thread_create_kernel(thread_idle); }

void thread_exit(void) {
  __asm__ volatile("cli");

  thread_t *current = thread_current();

  if (current == 0) {
    panic("thread_exit: no current thread");
  }

  current->state = THREAD_TERMINATED;
  current->interrupt_rsp = 0;

  thread_t *next = scheduler_next();

  if (next == 0) {
    panic("thread_exit: no runnable thread");
  }

  uint64_t new_cr3;

  if (next->process != 0) {
    if (next->process->address_space == 0) {
      panic("thread_exit: process has no address space");
    }

    new_cr3 = next->process->address_space->page_table_root;
  } else {
    new_cr3 = cpu_read_cr3() & PAGE_ADDRESS_MASK;
  }

  thread_set_current(next);
  next->state = THREAD_RUNNING;

  if (next->interrupt_rsp != 0) {
    uint64_t frame = next->interrupt_rsp;
    next->interrupt_rsp = 0;

    resume_preempted(frame, new_cr3);
  }

  context_switch(&current->saved_rsp, next->saved_rsp, new_cr3);

  panic("thread_exit: context_switch returned");
}
