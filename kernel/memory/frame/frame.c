#include "memory/frame/frame.h"
#include "memory/buddy/buddy.h"
#include "memory/paging/paging.h"

static frame_metadata_t *frame_metadata;

physical_frame_t frame_alloc(void) { return buddy_alloc(0); }

void frame_free(physical_frame_t frame) { buddy_free(frame); }

void frame_init(uint64_t metadata_address) {
  frame_metadata = (frame_metadata_t *)(uintptr_t)paging_physical_to_virtual(
      metadata_address);
}

frame_metadata_t *frame_get_metadata(physical_frame_t frame) {
  return &frame_metadata[frame];
}
