#include "physical_memory.h"
#include "drivers/terminal/terminal.h"
#include "frame.h"
#include "memblock.h"
#include "paging.h"
#include <stdint.h>

static physical_memory_region_t memory_regions[PHYSICAL_MEMORY_MAX_REGIONS];
static uint32_t memory_region_count_value = 0;
static uint64_t highest_available_physical_address = 0;
static physical_frame_t physical_frame_count = 0;

void physical_memory_init(const multiboot_info_t *multiboot_info) {
  memblock_init();
  memory_region_count_value = 0;
  highest_available_physical_address = 0;
  physical_frame_count = 0;

  if (multiboot_info == 0) {
    return;
  }

  if ((multiboot_info->flags & MULTIBOOT_INFO_FLAG_MEMORY_MAP) == 0) {
    return;
  }

  uint32_t offset = 0;

  while (offset + sizeof(uint32_t) <= multiboot_info->mmap_length) {
    multiboot_memory_map_entry_t *entry =
        (multiboot_memory_map_entry_t *)(uintptr_t)(multiboot_info->mmap_addr +
                                                    offset);

    if (entry->size < 20) {
      break;
    }

    uint64_t entry_size = (uint64_t)entry->size + sizeof(entry->size);
    if (entry_size + offset > multiboot_info->mmap_length) {
      break;
    }

    if (memory_region_count_value >= PHYSICAL_MEMORY_MAX_REGIONS) {
      terminal_write("Warning: memory map truncated\n");
      break;
    }

    memory_regions[memory_region_count_value].base = entry->addr;
    memory_regions[memory_region_count_value].length = entry->length;
    memory_regions[memory_region_count_value].type = entry->type;

    memory_region_count_value++;

    if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
      uint64_t region_end = entry->addr + entry->length;
      if (region_end > highest_available_physical_address) {
        highest_available_physical_address = region_end;
      }
    }
    offset += (uint32_t)entry_size;
  }

  physical_frame_count = highest_available_physical_address / PAGE_SIZE;
  memblock_reserve_boot();
  memblock_reserve_kernel();
  memblock_reserve_multiboot(multiboot_info);

  uint64_t frame_metadata_size =
      align_up(physical_frame_count * sizeof(frame_metadata_t));

  uint64_t frame_metadata_physical_address =
      memblock_alloc(frame_metadata_size, PAGE_SIZE);

  if (frame_metadata_physical_address == MEMBLOCK_INVALID_ADDRESS) {
    terminal_write("Failed to allocate frame metadata\n");

    for (;;) {
      __asm__ volatile("cli");
      __asm__ volatile("hlt");
    }
  }

  frame_init(frame_metadata_physical_address);
}

uint32_t physical_memory_region_count(void) {
  return memory_region_count_value;
}

uint64_t physical_memory_frame_count(void) { return physical_frame_count; }

const physical_memory_region_t *physical_memory_get_region(uint32_t index) {
  if (index >= memory_region_count_value) {
    return 0;
  }
  return &memory_regions[index];
}
