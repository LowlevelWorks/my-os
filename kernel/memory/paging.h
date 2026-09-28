#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>

#define PAGE_SIZE 4096ULL

#define PAGE_2M_SIZE (2ULL * 1024 * 1024)
#define PAGE_1G_SIZE (1024ULL * 1024 * 1024)

#define PHYS_MAP_BASE 0xFFFF800000000000ULL
#define PAGING_TEMP_BASE 0xFFFFFE0000000000ULL

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER (1ULL << 2)
#define PAGE_HUGE (1ULL << 7)
#define PAGE_NO_EXEC (1ULL << 63)

#define PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL

typedef uint64_t page_table_entry_t;

typedef struct {
  page_table_entry_t entries[512];
} page_table_t;

void paging_init(void);
void paging_map(uint64_t virtual_address, uint64_t physical_address,
                uint64_t flags);
void paging_unmap(uint64_t virtual_address);

static inline void *physical_to_virtual(uint64_t physical_address) {
  return (void *)(uintptr_t)(PHYS_MAP_BASE + physical_address);
}

void *paging_map_temporary(uint64_t physical_address);

static inline uint64_t align_up(uint64_t address) {
  return (address + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static inline uint64_t align_down(uint64_t address) {
  return address & ~(PAGE_SIZE - 1);
}

#endif
