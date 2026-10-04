#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_SIZE 0x1000ULL
#define PAGE_2M_SIZE 0x200000ULL
#define PAGE_1G_SIZE 0x40000000ULL

#define PAGING_ENTRIES 512ULL

#define PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER (1ULL << 2)
#define PAGE_PWT (1ULL << 3)
#define PAGE_PCD (1ULL << 4)
#define PAGE_HUGE (1ULL << 7)
#define PAGE_GLOBAL (1ULL << 8)
#define PAGE_NX (1ULL << 63)

#define PHYS_MAP_BASE 0xFFFF800000000000ULL
#define PHYS_MAP_SIZE 0x0000008000000000ULL

typedef uint64_t page_table_entry_t;

typedef struct {
  page_table_entry_t entries[PAGING_ENTRIES];
} page_table_t;

typedef enum {
  PAGING_OK = 0,
  PAGING_ERR_INVALID_ADDRESS,
  PAGING_ERR_INVALID_ALIGNMENT,
  PAGING_ERR_INVALID_RANGE,
  PAGING_ERR_INVALID_FLAGS,
  PAGING_ERR_ALREADY_MAPPED,
  PAGING_ERR_NOT_MAPPED,
  PAGING_ERR_HUGE_PAGE,
  PAGING_ERR_OUT_OF_MEMORY,
  PAGING_ERR_UNSUPPORTED,
} paging_result_t;

typedef enum {
  PAGING_PAGE_4K = 0,
  PAGING_PAGE_2M,
  PAGING_PAGE_1G,
} paging_page_size_t;

static inline uint64_t align_down(uint64_t address) {
  return address & ~(PAGE_SIZE - 1);
}

static inline uint64_t align_up(uint64_t address) {
  if (address > UINT64_MAX - (PAGE_SIZE - 1)) {
    return UINT64_MAX;
  }

  return (address + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

void paging_init(void);

paging_result_t paging_map(uint64_t virtual_address, uint64_t physical_address,
                           uint64_t flags);

paging_result_t paging_map_2m(uint64_t virtual_address,
                              uint64_t physical_address, uint64_t flags);

paging_result_t paging_map_1g(uint64_t virtual_address,
                              uint64_t physical_address, uint64_t flags);

paging_result_t paging_map_range(uint64_t virtual_address,
                                 uint64_t physical_address, uint64_t size,
                                 uint64_t flags);

paging_result_t paging_unmap(uint64_t virtual_address);

paging_result_t paging_unmap_2m(uint64_t virtual_address);

paging_result_t paging_unmap_1g(uint64_t virtual_address);

paging_result_t paging_translate(uint64_t virtual_address,
                                 uint64_t *physical_address,
                                 paging_page_size_t *page_size);

paging_result_t paging_map_in_address_space(uint64_t page_table_root,
                                            uint64_t virtual_address,
                                            uint64_t physical_address,
                                            uint64_t flags);

paging_result_t paging_map_in_address_space(uint64_t page_table_root,
                                            uint64_t virtual_address,
                                            uint64_t physical_address,
                                            uint64_t flags);

paging_result_t paging_translate_in_address_space(
    uint64_t page_table_root, uint64_t virtual_address,
    uint64_t *physical_address, paging_page_size_t *page_size);

paging_result_t paging_protect(uint64_t virtual_address, uint64_t flags);

static inline uint64_t paging_physical_to_virtual(uint64_t physical_address) {
  return PHYS_MAP_BASE + physical_address;
}

static inline uint64_t paging_virtual_to_physical(uint64_t virtual_address) {
  return virtual_address - PHYS_MAP_BASE;
}

int paging_is_canonical(uint64_t virtual_address);

void paging_dump(uint64_t virtual_address);

#endif
