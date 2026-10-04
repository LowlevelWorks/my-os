#ifndef PIT_H
#define PIT_H

#include <stdint.h>

#include "interrupt_frame.h"

void pit_init(uint32_t frequency);

uint64_t pit_ticks(void);

void pit_handle_irq(void);

void pit_handle_irq_frame(interrupt_frame_t *frame);

#endif
