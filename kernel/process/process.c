#include "process.h"
#include "memory/address_space/address_space.h"
#include "memory/frame/frame.h"
#include "memory/paging/paging.h"
#include "panic/panic.h"
#include "process/scheduler.h"
#include "process/thread.h"

static process_id_t next_process_id = 1;

void process_init(void) { next_process_id = 1; }

void process_add_thread(process_t *process, thread_t *thread) {
  if (process == 0) {
    panic("process_add_thread: null process");
  }

  if (thread == 0) {
    panic("process_add_thread: null thread");
  }

  if (thread->process != process) {
    panic("process_add_thread: thread belongs to another process");
  }

  if (thread->process_next != 0) {
    panic("process_add_thread: thread already linked");
  }

  thread->process_next = 0;

  if (process->threads == 0) {
    process->threads = thread;
    process->threads_tail = thread;
    process->main_thread = thread;
    return;
  }

  process->threads_tail->process_next = thread;
  process->threads_tail = thread;
}

process_t *process_create_kernel(void) {
  physical_frame_t frame = frame_alloc();

  if (frame == INVALID_FRAME) {
    panic("process_create_kernel: unable to allocate process");
  }

  uint64_t physical_address = frame_to_address(frame);

  process_t *process =
      (process_t *)paging_physical_to_virtual(physical_address);

  process->id = next_process_id++;
  process->state = PROCESS_NEW;
  process->address_space = address_space_create_kernel();
  process->main_thread = 0;
  process->threads = 0;
  process->threads_tail = 0;

  if (process->address_space == 0) {
    panic("process_create_kernel: unable to create address space");
  }

  return process;
}

thread_t *process_create_thread(process_t *process, void (*entry)(void)) {
  if (process == 0) {
    panic("process_create_thread: null process");
  }

  if (process->state != PROCESS_NEW) {
    panic("process_create_thread: process is already started");
  }

  if (process->address_space == 0) {
    panic("process_create_thread: process has no address space");
  }

  if (entry == 0) {
    panic("process_create_thread: null entry");
  }

  thread_t *thread = thread_create_kernel_for_process(process, entry);

  if (thread == 0) {
    panic("process_create_thread: unable to create thread");
  }

  process_add_thread(process, thread);

  return thread;
}

void process_start(process_t *process) {
  if (process == 0) {
    panic("process_start: null process");
  }

  if (process->state != PROCESS_NEW) {
    panic("process_start: invalid process start");
  }

  if (process->threads == 0) {
    panic("process_start: process has no threads");
  }

  process->state = PROCESS_READY;

  for (thread_t *thread = process->threads; thread != 0;
       thread = thread->process_next) {
    scheduler_add(thread);
  }
}
