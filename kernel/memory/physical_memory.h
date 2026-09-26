#ifndef PHYSICAL_MEMORY_H
#define PHYSICAL_MEMORY_H

#include "arch/x86_64/multiboot.h"
#include <stdint.h>

#define PHYSICAL_MEMORY_MAX_REGIONS 128

typedef struct {
  uint64_t base;
  uint64_t length;
  uint32_t type;
} physical_memory_region_t;

void physical_memory_init(const multiboot_info_t *multiboot_info);

uint32_t physical_memory_region_count(void);
uint64_t physical_memory_frame_count(void);

const physical_memory_region_t *physical_memory_get_region(uint32_t index);

#endif
