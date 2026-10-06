#include "process/wait_queue/wait_queue.h"

#include "panic/panic.h"
#include "process/scheduler/scheduler.h"

void wait_queue_init(wait_queue_t *queue) {
  if (queue == 0)
    panic("wait_queue_init: null queue");

  queue->head = 0;
  queue->tail = 0;
}

void wait_queue_add(wait_queue_t *queue, struct thread *thread) {
  if (queue == 0)
    panic("wait_queue_add: null queue");

  if (thread == 0)
    panic("wait_queue_add: null thread");

  if (thread->state != THREAD_BLOCKED)
    panic("wait_queue_add: thread is not blocked");

  thread->next = 0;

  if (queue->tail == 0) {
    queue->head = thread;
    queue->tail = thread;
    return;
  }

  queue->tail->next = thread;
  queue->tail = thread;
}

struct thread *wait_queue_remove(wait_queue_t *queue, struct thread *thread) {
  if (queue == 0)
    panic("wait_queue_remove: null queue");

  if (thread == 0)
    panic("wait_queue_remove: null thread");

  struct thread *previous = 0;
  struct thread *current = queue->head;

  while (current != 0) {
    if (current == thread) {
      if (previous == 0)
        queue->head = current->next;
      else
        previous->next = current->next;

      if (queue->tail == current)
        queue->tail = previous;

      current->next = 0;

      return current;
    }

    previous = current;
    current = current->next;
  }

  return 0;
}

struct thread *wait_queue_wake_one(wait_queue_t *queue) {
  if (queue == 0)
    panic("wait_queue_wake_one: null queue");

  struct thread *thread = queue->head;

  if (thread == 0)
    return 0;

  queue->head = thread->next;

  if (queue->head == 0)
    queue->tail = 0;

  thread->next = 0;

  scheduler_wake(thread);

  return thread;
}

void wait_queue_wake_all(wait_queue_t *queue) {
  if (queue == 0)
    panic("wait_queue_wake_all: null queue");

  while (queue->head != 0)
    wait_queue_wake_one(queue);
}
