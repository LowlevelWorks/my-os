#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#include "process/wait_queue/wait_queue.h"

struct thread;
struct address_space;

typedef uint64_t process_id_t;

typedef enum {
  PROCESS_NEW,
  PROCESS_READY,
  PROCESS_RUNNING,
  PROCESS_BLOCKED,
  PROCESS_ZOMBIE
} process_state_t;

typedef struct process process_t;

struct process {
  process_id_t id;
  process_state_t state;

  int exit_status;

  process_t *parent;
  process_t *children;
  process_t *children_tail;
  process_t *sibling_next;

  struct address_space *address_space;

  struct thread *main_thread;
  struct thread *threads;
  struct thread *threads_tail;

  wait_queue_t child_wait_queue;
};

void process_init(void);

process_t *process_create_kernel(void);
process_t *process_create(process_t *parent);

struct thread *process_create_thread(process_t *process, void (*entry)(void));

void process_add_thread(process_t *process, struct thread *thread);

int process_has_live_threads(process_t *process);

void process_start(process_t *process);

int process_wait(process_t *parent, int *status);

void process_reparent_children(process_t *process);

void process_init_reaper(void);
process_t *process_get_reaper(void);

#endif
