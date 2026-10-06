#ifndef WAIT_QUEUE_H
#define WAIT_QUEUE_H

#include "process/thread/thread.h"

struct thread;

typedef struct wait_queue {
  thread_t *head;
  thread_t *tail;
} wait_queue_t;

void wait_queue_init(wait_queue_t *queue);
void wait_queue_add(wait_queue_t *queue, struct thread *thread);
struct thread *wait_queue_remove(wait_queue_t *queue, struct thread *thread);

struct thread *wait_queue_wake_one(wait_queue_t *queue);

void wait_queue_wake_all(wait_queue_t *queue);

#endif
