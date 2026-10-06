#ifndef SLEEP_H
#define SLEEP_H

#include <stdint.h>

void sleep_init(void);

void thread_sleep(uint64_t ticks);

void sleep_tick(uint64_t current_tick);

#endif
