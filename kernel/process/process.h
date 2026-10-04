#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

struct thread;
struct address_space;

typedef uint64_t process_id_t;

typedef enum {
  PROCESS_NEW,
  PROCESS_READY,
  PROCESS_RUNNING,
  PROCESS_BLOCKED,
  PROCESS_TERMINATED
} process_state_t;

typedef struct process process_t;

struct process {
  process_id_t id;
  process_state_t state;

  struct address_space *address_space;

  struct thread *main_thread;
  struct thread *threads;
  struct thread *threads_tail;
};

void process_init(void);

process_t *process_create_kernel(void);

struct thread *process_create_thread(process_t *process, void (*entry)(void));

void process_add_thread(process_t *process, struct thread *thread);

void process_start(process_t *process);

#endif
