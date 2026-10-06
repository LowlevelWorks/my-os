#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

struct interrupt_frame;
struct thread;

void scheduler_init(void);

void scheduler_add(struct thread *thread);

struct thread *scheduler_next(void);

void scheduler_start(void);

void scheduler_create_idle(void);

void scheduler_tick(struct interrupt_frame *frame);

void scheduler_block_current(void);
void scheduler_wake(struct thread *thread);

int scheduler_interrupt_switch_required(void);

uint64_t scheduler_interrupt_next_rsp(void);

uint64_t scheduler_interrupt_next_cr3(void);

int scheduler_interrupt_next_is_preempted(void);

#endif
