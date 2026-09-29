#ifndef BUDDY_H
#define BUDDY_H

#include "memory/frame/frame.h"

#define BUDDY_MAX_ORDER 10

void buddy_init(void);

physical_frame_t buddy_alloc(unsigned int order);
void buddy_free(physical_frame_t frame);
uint64_t buddy_free_frame_count(void);
void buddy_debug_print(void);

#endif
