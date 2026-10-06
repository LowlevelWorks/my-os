#include "process.h"

#include "memory/address_space/address_space.h"
#include "memory/frame/frame.h"
#include "memory/paging/paging.h"
#include "panic/panic.h"
#include "process/scheduler/scheduler.h"
#include "process/thread/thread.h"

static process_id_t next_process_id = 1;
static process_t *reaper_process = 0;

void process_init(void) {
  next_process_id = 1;
  reaper_process = 0;
}

static process_t *process_allocate(process_t *parent) {
  physical_frame_t frame = frame_alloc();

  if (frame == INVALID_FRAME)
    panic("process_allocate: unable to allocate process");

  process_t *process =
      (process_t *)paging_physical_to_virtual(frame_to_address(frame));

  process->id = next_process_id++;
  process->state = PROCESS_NEW;
  process->exit_status = 0;

  process->parent = parent;
  process->children = 0;
  process->children_tail = 0;
  process->sibling_next = 0;

  process->address_space = address_space_create_kernel();

  process->main_thread = 0;
  process->threads = 0;
  process->threads_tail = 0;

  wait_queue_init(&process->child_wait_queue);

  if (process->address_space == 0)
    panic("process_allocate: unable to create address space");

  return process;
}

static void process_add_child(process_t *parent, process_t *child) {
  if (parent == 0)
    panic("process_add_child: null parent");

  if (child == 0)
    panic("process_add_child: null child");

  if (child->parent != parent)
    panic("process_add_child: invalid parent");

  child->sibling_next = 0;

  if (parent->children == 0) {
    parent->children = child;
    parent->children_tail = child;
    return;
  }

  parent->children_tail->sibling_next = child;
  parent->children_tail = child;
}

void process_add_thread(process_t *process, thread_t *thread) {
  if (process == 0)
    panic("process_add_thread: null process");

  if (thread == 0)
    panic("process_add_thread: null thread");

  if (thread->process != process)
    panic("process_add_thread: thread belongs to another process");

  if (thread->process_next != 0)
    panic("process_add_thread: thread already linked");

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

process_t *process_create_kernel(void) { return process_allocate(0); }

process_t *process_create(process_t *parent) {
  if (parent == 0)
    panic("process_create: null parent");

  process_t *process = process_allocate(parent);

  process_add_child(parent, process);

  return process;
}

thread_t *process_create_thread(process_t *process, void (*entry)(void)) {
  if (process == 0)
    panic("process_create_thread: null process");

  if (process->state != PROCESS_NEW)
    panic("process_create_thread: process is already started");

  if (process->address_space == 0)
    panic("process_create_thread: process has no address space");

  if (entry == 0)
    panic("process_create_thread: null entry");

  thread_t *thread = thread_create_kernel_for_process(process, entry);

  process_add_thread(process, thread);

  return thread;
}

void process_start(process_t *process) {
  if (process == 0)
    panic("process_start: null process");

  if (process->state != PROCESS_NEW)
    panic("process_start: invalid process start");

  if (process->threads == 0)
    panic("process_start: process has no threads");

  process->state = PROCESS_READY;

  for (thread_t *thread = process->threads; thread != 0;
       thread = thread->process_next) {
    scheduler_add(thread);
  }
}

int process_has_live_threads(process_t *process) {
  if (process == 0)
    panic("process_has_live_threads: null process");

  for (thread_t *thread = process->threads; thread != 0;
       thread = thread->process_next) {
    if (thread->state != THREAD_TERMINATED)
      return 1;
  }

  return 0;
}

static void process_remove_child(process_t *parent, process_t *child) {
  process_t *previous = 0;
  process_t *current = parent->children;

  while (current != 0) {
    if (current == child) {
      if (previous == 0)
        parent->children = current->sibling_next;
      else
        previous->sibling_next = current->sibling_next;

      if (parent->children_tail == child)
        parent->children_tail = previous;

      child->sibling_next = 0;
      return;
    }

    previous = current;
    current = current->sibling_next;
  }

  panic("process_remove_child: child not found");
}

static void process_destroy_threads(process_t *process) {
  thread_t *thread = process->threads;

  while (thread != 0) {
    thread_t *next = thread->process_next;

    thread_destroy(thread);

    thread = next;
  }

  process->threads = 0;
  process->threads_tail = 0;
  process->main_thread = 0;
}

void process_reparent_children(process_t *process) {
  if (process == 0)
    panic("process_reparent_children: null process");

  process_t *reaper = process_get_reaper();

  if (reaper == process)
    panic("process_reparent_children: reaper cannot reparent itself");

  process_t *child = process->children;

  process->children = 0;
  process->children_tail = 0;

  while (child != 0) {
    process_t *next = child->sibling_next;

    child->sibling_next = 0;
    child->parent = reaper;

    process_add_child(reaper, child);

    if (child->state == PROCESS_ZOMBIE)
      wait_queue_wake_one(&reaper->child_wait_queue);

    child = next;
  }
}

int process_wait(process_t *parent, int *status) {
  if (parent == 0)
    panic("process_wait: null parent");

  for (;;) {
    process_t *child = parent->children;

    while (child != 0) {
      if (child->state == PROCESS_ZOMBIE) {
        process_id_t child_id = child->id;

        if (status != 0)
          *status = child->exit_status;

        process_remove_child(parent, child);
        process_destroy_threads(child);

        address_space_destroy(child->address_space);

        physical_frame_t process_frame =
            address_to_frame(paging_virtual_to_physical((uint64_t)child));

        frame_free(process_frame);

        return (int)child_id;
      }

      child = child->sibling_next;
    }

    if (parent->children == 0)
      return -1;

    thread_wait(&parent->child_wait_queue);
  }
}

void process_init_reaper(void) {
  if (reaper_process != 0)
    panic("process_init_reaper: reaper already initialized");

  reaper_process = process_create_kernel();
}

process_t *process_get_reaper(void) {
  if (reaper_process == 0)
    panic("process_get_reaper: reaper not initialized");

  return reaper_process;
}
