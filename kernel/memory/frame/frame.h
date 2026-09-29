#ifndef FRAME_H
#define FRAME_H
#include "memory/paging/paging.h"
#include <stdint.h>

typedef uint64_t physical_frame_t;

typedef uint8_t frame_state_t;

#define FRAME_FREE 0
#define FRAME_USED 1

#define INVALID_FRAME UINT64_MAX

typedef struct {
  frame_state_t state;
  uint8_t order;
  physical_frame_t next;
} frame_metadata_t;

void frame_init(uint64_t metadata_address);
frame_metadata_t *frame_get_metadata(physical_frame_t frame);

physical_frame_t frame_alloc(void);
void frame_free(physical_frame_t frame);

static inline uint64_t frame_to_address(physical_frame_t frame) {
  return frame * PAGE_SIZE;
}

static inline physical_frame_t address_to_frame(uint64_t address) {
  return address / PAGE_SIZE;
}

#endif
