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
#include "process/process/process.h"
#include "process/scheduler/scheduler.h"
#include "process/wait_queue/wait_queue.h"

#define THREAD_STACK_ORDER 2
#define THREAD_STACK_FRAMES (1ULL << THREAD_STACK_ORDER)
#define THREAD_STACK_SIZE (THREAD_STACK_FRAMES * PAGE_SIZE)

static thread_id_t next_thread_id = 1;

static uint64_t thread_cr3(thread_t *thread) {
  if (thread == 0)
    panic("thread_cr3: null thread");

  if (thread->process == 0)
    return cpu_read_cr3() & PAGE_ADDRESS_MASK;

  if (thread->process->address_space == 0)
    panic("thread_cr3: process has no address space");

  return thread->process->address_space->page_table_root;
}

thread_t *thread_current(void) { return cpu_current_thread(); }

void thread_set_current(thread_t *thread) { cpu_set_current_thread(thread); }

static void thread_trampoline(void) {
  __asm__ volatile("sti");

  thread_t *current = thread_current();

  if (current == 0)
    panic("thread_trampoline: no current thread");

  if (current->entry == 0)
    panic("thread_trampoline: no entry");

  current->entry();

  thread_exit(0);

  panic("thread_trampoline: thread_exit returned");
}

void thread_init(void) {
  next_thread_id = 1;
  thread_set_current(0);
}

thread_t *thread_create_kernel_for_process(struct process *process,
                                           void (*entry)(void)) {
  if (entry == 0)
    panic("thread_create_kernel_for_process: null entry");

  if (process != 0 && process->address_space == 0)
    panic("thread_create_kernel_for_process: process has no address space");

  physical_frame_t thread_frame = frame_alloc();

  if (thread_frame == INVALID_FRAME)
    panic("thread_create_kernel_for_process: unable to allocate thread");

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
  for (;;)
    __asm__ volatile("hlt");
}

thread_t *thread_create_idle(void) { return thread_create_kernel(thread_idle); }

void thread_block(void) {
  scheduler_block_current();

  __asm__ volatile("sti");
}

void thread_wake(thread_t *thread) { scheduler_wake(thread); }

void thread_wait(wait_queue_t *queue) {
  if (queue == 0)
    panic("thread_wait: null queue");

  __asm__ volatile("cli");

  thread_t *current = thread_current();

  if (current == 0)
    panic("thread_wait: no current thread");

  if (current->state != THREAD_RUNNING)
    panic("thread_wait: thread is not running");

  current->state = THREAD_BLOCKED;
  wait_queue_add(queue, current);

  scheduler_block_current();

  __asm__ volatile("sti");
}

void thread_exit(int status) {
  __asm__ volatile("cli");

  thread_t *current = thread_current();

  if (current == 0)
    panic("thread_exit: no current thread");

  process_t *process = current->process;

  current->state = THREAD_TERMINATED;
  current->interrupt_rsp = 0;

  if (process != 0 && !process_has_live_threads(process)) {
    if (process != process_get_reaper()) {
      process_reparent_children(process);

      process->exit_status = status;
      process->state = PROCESS_ZOMBIE;

      if (process->parent != 0)
        wait_queue_wake_one(&process->parent->child_wait_queue);
    }
  }

  thread_t *next = scheduler_next();

  if (next == 0)
    panic("thread_exit: no runnable thread");

  thread_set_current(next);
  next->state = THREAD_RUNNING;

  uint64_t new_cr3 = thread_cr3(next);

  if (next->interrupt_rsp != 0) {
    uint64_t frame = next->interrupt_rsp;
    next->interrupt_rsp = 0;

    resume_preempted(frame, new_cr3);
  }

  context_switch(&current->saved_rsp, next->saved_rsp, new_cr3);

  panic("thread_exit: context_switch returned");
}

void thread_destroy(thread_t *thread) {
  if (thread == 0)
    panic("thread_destroy: null thread");

  if (thread->state != THREAD_TERMINATED)
    panic("thread_destroy: thread is not terminated");

  physical_frame_t stack_frame =
      address_to_frame(paging_virtual_to_physical(thread->kernel_stack_base));

  physical_frame_t thread_frame =
      address_to_frame(paging_virtual_to_physical((uint64_t)thread));

  buddy_free(stack_frame);
  frame_free(thread_frame);
}
