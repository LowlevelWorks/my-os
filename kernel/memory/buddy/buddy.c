#include "buddy.h"
#include "drivers/terminal/terminal.h"
#include "memory/memblock/memblock.h"
#include "memory/paging/paging.h"
#include "memory/physical_memory/physical_memory.h"

static physical_frame_t free_lists[BUDDY_MAX_ORDER + 1];
static uint64_t free_block_counts[BUDDY_MAX_ORDER + 1];

static void buddy_add_free_block(physical_frame_t frame, unsigned int order) {
  frame_get_metadata(frame)->state = FRAME_FREE;
  frame_get_metadata(frame)->order = order;
  frame_get_metadata(frame)->next = free_lists[order];

  free_lists[order] = frame;
  free_block_counts[order]++;
}

static physical_frame_t buddy_remove_free_block_head(unsigned int order) {
  physical_frame_t frame = free_lists[order];

  if (frame == INVALID_FRAME) {
    return INVALID_FRAME;
  }

  free_lists[order] = frame_get_metadata(frame)->next;
  free_block_counts[order]--;

  frame_get_metadata(frame)->next = INVALID_FRAME;
  return frame;
}

static int buddy_remove_free_block(physical_frame_t frame, unsigned int order) {
  physical_frame_t current = free_lists[order];
  physical_frame_t previous = INVALID_FRAME;

  while (current != INVALID_FRAME) {
    if (current == frame) {
      if (previous == INVALID_FRAME) {
        free_lists[order] = frame_get_metadata(current)->next;
      } else {
        frame_get_metadata(previous)->next = frame_get_metadata(current)->next;
      }

      free_block_counts[order]--;
      return 1;
    }

    previous = current;
    current = frame_get_metadata(current)->next;
  }

  return 0;
}

static unsigned int buddy_get_max_order(physical_frame_t frame,
                                        physical_frame_t end_frame) {
  unsigned int order = 0;

  while (order < BUDDY_MAX_ORDER) {
    unsigned int next_order = order + 1;
    physical_frame_t block_size = 1ULL << next_order;

    if (frame % block_size != 0) {
      break;
    }

    if (block_size > end_frame - frame) {
      break;
    }
    order = next_order;
  }
  return order;
}

void buddy_init(void) {
  for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
    free_lists[order] = INVALID_FRAME;
    free_block_counts[order] = 0;
  }

  uint32_t region_count = physical_memory_region_count();
  for (uint32_t i = 0; i < region_count; i++) {
    const physical_memory_region_t *region = physical_memory_get_region(i);

    if (region == 0) {
      continue;
    }

    if (region->type != MULTIBOOT_MEMORY_AVAILABLE) {
      continue;
    }

    physical_frame_t start_frame = address_to_frame(align_up(region->base));

    physical_frame_t end_frame =
        address_to_frame(align_down(region->base + region->length));

    physical_frame_t frame = start_frame;

    while (frame < end_frame) {
      unsigned int order = buddy_get_max_order(frame, end_frame);

      physical_frame_t block_size;
      uint64_t reservation_end;
      for (;;) {
        block_size = 1ULL << order;
        uint64_t block_base = frame * PAGE_SIZE;
        uint64_t block_length = block_size * PAGE_SIZE;

        reservation_end =
            memblock_get_reservation_end(block_base, block_length);

        if (reservation_end == MEMBLOCK_INVALID_ADDRESS) {
          break;
        }

        if (order == 0) {
          break;
        }
        order--;
      }

      if (reservation_end != MEMBLOCK_INVALID_ADDRESS) {
        frame = address_to_frame(align_up(reservation_end));
        continue;
      }

      buddy_add_free_block(frame, order);
      frame += block_size;
    }
  }
}

physical_frame_t buddy_alloc(unsigned int order) {
  if (order > BUDDY_MAX_ORDER) {
    return INVALID_FRAME;
  }

  unsigned int current_order = order;

  while (current_order <= BUDDY_MAX_ORDER &&
         free_lists[current_order] == INVALID_FRAME) {
    current_order++;
  }

  if (current_order > BUDDY_MAX_ORDER) {
    return INVALID_FRAME;
  }

  physical_frame_t frame = buddy_remove_free_block_head(current_order);

  while (current_order > order) {
    current_order--;
    physical_frame_t buddy_frame = frame + (1ULL << current_order);

    buddy_add_free_block(buddy_frame, current_order);
  }
  frame_get_metadata(frame)->state = FRAME_USED;
  frame_get_metadata(frame)->order = order;
  frame_get_metadata(frame)->next = INVALID_FRAME;

  return frame;
}

static int buddy_is_block_head(physical_frame_t frame, unsigned int order) {
  physical_frame_t block_size = 1ULL << order;
  return (frame % block_size) == 0;
}

uint64_t buddy_free_frame_count(void) {
  uint64_t count = 0;

  for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
    count += free_block_counts[order] * (1ULL << order);
  }
  return count;
}

void buddy_free(physical_frame_t frame) {
  if (frame >= physical_memory_frame_count()) {
    return;
  }

  frame_metadata_t *metadata = frame_get_metadata(frame);

  if (metadata->state != FRAME_USED) {
    return;
  }

  unsigned int order = metadata->order;

  if (order > BUDDY_MAX_ORDER) {
    return;
  }

  if (!buddy_is_block_head(frame, order)) {
    return;
  }

  while (order < BUDDY_MAX_ORDER) {
    physical_frame_t buddy_frame = frame ^ (1ULL << order);

    if (!buddy_remove_free_block(buddy_frame, order)) {
      break;
    }

    frame_get_metadata(buddy_frame)->next = INVALID_FRAME;

    if (buddy_frame < frame) {
      frame = buddy_frame;
    }

    order++;
  }

  buddy_add_free_block(frame, order);
}

void buddy_debug_print(void) {
  for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
    terminal_write("order ");
    terminal_write_uint(order);
    terminal_write(": ");

    physical_frame_t frame = free_lists[order];

    while (frame != INVALID_FRAME) {
      terminal_write_uint(frame);
      terminal_write(" -> ");

      frame = frame_get_metadata(frame)->next;
    }

    terminal_write("INVALID\n");
  }
}
