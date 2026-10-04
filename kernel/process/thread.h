#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>

#define THREAD_KERNEL_STACK_SIZE (16 * 1024)

typedef uint64_t thread_id_t;

typedef enum {
  THREAD_NEW,
  THREAD_READY,
  THREAD_RUNNING,
  THREAD_BLOCKED,
  THREAD_TERMINATED,
} thread_state_t;

typedef struct thread thread_t;

struct process;

struct thread {
  thread_id_t id;
  thread_state_t state;

  uint64_t kernel_stack_base;
  uint64_t kernel_stack_top;

  uint64_t saved_rsp;

  uint64_t interrupt_rsp;

  void (*entry)(void);

  struct process *process;

  thread_t *next;
  thread_t *process_next;
};

void thread_init(void);

thread_t *thread_create_kernel(void (*entry)(void));

thread_t *thread_create_kernel_for_process(struct process *process,
                                           void (*entry)(void));

thread_t *thread_create_idle(void);

void thread_exit(void);

thread_t *thread_current(void);

void thread_set_current(thread_t *thread);

#endif
