#ifndef MEMBLOCK_H
#define MEMBLOCK_H

#include "multiboot.h"
#include <stdint.h>

#define MEMBLOCK_MAX_RESERVATIONS 128
#define MEMBLOCK_INVALID_ADDRESS (~0ULL)

typedef struct {
  uint64_t base;
  uint64_t length;
} memblock_reservation_t;

void memblock_init(void);

void memblock_reserve(uint64_t base, uint64_t length);

int memblock_range_is_reserved(uint64_t base, uint64_t length);

uint64_t memblock_get_reservation_end(uint64_t base, uint64_t length);

uint64_t memblock_alloc(uint64_t size, uint64_t alignment);

void memblock_reserve_kernel(void);

void memblock_reserve_boot(void);

void memblock_reserve_multiboot(const multiboot_info_t *multiboot_info);

#endif
