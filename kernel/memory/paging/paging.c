#include "paging.h"
#include "arch/x86_64/cpu.h"
#include "drivers/terminal/terminal.h"
#include "memory/buddy/buddy.h"
#include "memory/frame/frame.h"
#include "memory/physical_memory/physical_memory.h"
#include "paging_utils.h"
#include "panic/panic.h"

#include <stdint.h>

#define EARLY_TABLE_POOL_PAGES 64

#define KERNEL_VMA 0xFFFFFFFF80000000ULL

static page_table_t early_table_pool[EARLY_TABLE_POOL_PAGES]
    __attribute__((aligned(PAGE_SIZE)));

static uint64_t early_table_pool_used;
static int direct_map_ready;
static int supports_1g_pages;

extern page_table_t pml4_table;

extern uint64_t kernel_load_start_phys(void);

static paging_result_t paging_validate_mapping(uint64_t virtual_address,
                                               uint64_t physical_address,
                                               uint64_t size, uint64_t flags) {
  if (!paging_is_canonical(virtual_address)) {
    return PAGING_ERR_INVALID_ADDRESS;
  }

  if (!paging_physical_range_valid(physical_address, size)) {
    return PAGING_ERR_INVALID_ADDRESS;
  }

  if ((virtual_address & (size - 1)) != 0 ||
      (physical_address & (size - 1)) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  return paging_validate_flags(flags);
}

static uint64_t paging_early_table_physical(page_table_t *table) {
  uint64_t virtual_address = (uint64_t)table;
  uint64_t kernel_load_start = kernel_load_start_phys();

  return virtual_address - KERNEL_VMA + kernel_load_start;
}

static void paging_clear_table(page_table_t *table) {
  for (uint64_t i = 0; i < PAGING_ENTRIES; i++) {
    table->entries[i] = 0;
  }
}

static uint64_t paging_alloc_table(void) {
  if (!direct_map_ready) {
    if (early_table_pool_used >= EARLY_TABLE_POOL_PAGES) {
      panic("early page-table pool exhausted");
    }

    page_table_t *table = &early_table_pool[early_table_pool_used++];

    paging_clear_table(table);

    return paging_early_table_physical(table);
  }

  physical_frame_t frame = buddy_alloc(0);

  if (frame == INVALID_FRAME) {
    panic("unable to allocate page-table frame");
  }

  uint64_t physical_address = frame_to_address(frame);

  page_table_t *table =
      (page_table_t *)paging_physical_to_virtual(physical_address);

  paging_clear_table(table);

  return physical_address;
}

static page_table_t *paging_get_table(page_table_t *parent, uint64_t index,
                                      int user) {
  uint64_t entry = parent->entries[index];

  if (entry & PAGE_HUGE) {
    return 0;
  }

  if (!(entry & PAGE_PRESENT)) {
    uint64_t physical = paging_alloc_table();

    uint64_t flags = PAGE_PRESENT | PAGE_WRITABLE;

    if (user) {
      flags |= PAGE_USER;
    }

    parent->entries[index] = paging_make_entry(physical, flags);

    entry = parent->entries[index];
  } else if (user && !(entry & PAGE_USER)) {
    parent->entries[index] |= PAGE_USER;
    entry = parent->entries[index];
  }

  return (page_table_t *)paging_physical_to_virtual(
      paging_entry_address(entry));
}

static paging_result_t paging_lookup(page_table_t *pml4,
                                     uint64_t virtual_address,
                                     page_table_entry_t **entry,
                                     paging_page_size_t *page_size) {
  if (pml4 == 0) {
    return PAGING_ERR_INVALID_ADDRESS;
  }

  if (!paging_is_canonical(virtual_address)) {
    return PAGING_ERR_INVALID_ADDRESS;
  }

  uint64_t pml4e = pml4->entries[paging_pml4_index(virtual_address)];

  if (!(pml4e & PAGE_PRESENT)) {
    return PAGING_ERR_NOT_MAPPED;
  }

  page_table_t *pdpt =
      (page_table_t *)paging_physical_to_virtual(paging_entry_address(pml4e));

  uint64_t pdpte = pdpt->entries[paging_pdpt_index(virtual_address)];

  if (!(pdpte & PAGE_PRESENT)) {
    return PAGING_ERR_NOT_MAPPED;
  }

  if (pdpte & PAGE_HUGE) {
    *entry = &pdpt->entries[paging_pdpt_index(virtual_address)];

    if (page_size) {
      *page_size = PAGING_PAGE_1G;
    }

    return PAGING_OK;
  }

  page_table_t *pd =
      (page_table_t *)paging_physical_to_virtual(paging_entry_address(pdpte));

  uint64_t pde = pd->entries[paging_pd_index(virtual_address)];

  if (!(pde & PAGE_PRESENT)) {
    return PAGING_ERR_NOT_MAPPED;
  }

  if (pde & PAGE_HUGE) {
    *entry = &pd->entries[paging_pd_index(virtual_address)];

    if (page_size) {
      *page_size = PAGING_PAGE_2M;
    }

    return PAGING_OK;
  }

  page_table_t *pt =
      (page_table_t *)paging_physical_to_virtual(paging_entry_address(pde));

  uint64_t pte = pt->entries[paging_pt_index(virtual_address)];

  if (!(pte & PAGE_PRESENT)) {
    return PAGING_ERR_NOT_MAPPED;
  }

  *entry = &pt->entries[paging_pt_index(virtual_address)];

  if (page_size) {
    *page_size = PAGING_PAGE_4K;
  }

  return PAGING_OK;
}
static paging_result_t paging_map_4k_internal(page_table_t *pml4,
                                              uint64_t virtual_address,
                                              uint64_t physical_address,
                                              uint64_t flags) {
  paging_result_t result = paging_validate_mapping(
      virtual_address, physical_address, PAGE_SIZE, flags);

  if (result != PAGING_OK) {
    return result;
  }

  page_table_t *pdpt = paging_get_table(
      pml4, paging_pml4_index(virtual_address), (flags & PAGE_USER) != 0);

  if (!pdpt) {
    return PAGING_ERR_HUGE_PAGE;
  }

  page_table_t *pd = paging_get_table(pdpt, paging_pdpt_index(virtual_address),
                                      (flags & PAGE_USER) != 0);

  if (!pd) {
    return PAGING_ERR_HUGE_PAGE;
  }

  page_table_t *pt = paging_get_table(pd, paging_pd_index(virtual_address),
                                      (flags & PAGE_USER) != 0);

  if (!pt) {
    return PAGING_ERR_HUGE_PAGE;
  }

  uint64_t index = paging_pt_index(virtual_address);

  if (pt->entries[index] & PAGE_PRESENT) {
    return PAGING_ERR_ALREADY_MAPPED;
  }

  pt->entries[index] =
      paging_make_entry(physical_address, flags | PAGE_PRESENT);

  cpu_invlpg(virtual_address);

  return PAGING_OK;
}

static paging_result_t paging_map_2m_internal(page_table_t *pml4,
                                              uint64_t virtual_address,
                                              uint64_t physical_address,
                                              uint64_t flags) {
  paging_result_t result = paging_validate_mapping(
      virtual_address, physical_address, PAGE_2M_SIZE, flags);

  if (result != PAGING_OK) {
    return result;
  }

  page_table_t *pdpt = paging_get_table(
      pml4, paging_pml4_index(virtual_address), (flags & PAGE_USER) != 0);

  if (!pdpt) {
    return PAGING_ERR_HUGE_PAGE;
  }

  page_table_t *pd = paging_get_table(pdpt, paging_pdpt_index(virtual_address),
                                      (flags & PAGE_USER) != 0);

  if (!pd) {
    return PAGING_ERR_HUGE_PAGE;
  }

  uint64_t index = paging_pd_index(virtual_address);

  if (pd->entries[index] & PAGE_PRESENT) {
    return PAGING_ERR_ALREADY_MAPPED;
  }

  pd->entries[index] =
      paging_make_entry(physical_address, flags | PAGE_PRESENT | PAGE_HUGE);

  cpu_invlpg(virtual_address);

  return PAGING_OK;
}

static paging_result_t paging_map_1g_internal(page_table_t *pml4,
                                              uint64_t virtual_address,
                                              uint64_t physical_address,
                                              uint64_t flags) {
  if (!supports_1g_pages) {
    return PAGING_ERR_UNSUPPORTED;
  }

  paging_result_t result = paging_validate_mapping(
      virtual_address, physical_address, PAGE_1G_SIZE, flags);

  if (result != PAGING_OK) {
    return result;
  }

  page_table_t *pdpt = paging_get_table(
      pml4, paging_pml4_index(virtual_address), (flags & PAGE_USER) != 0);

  if (!pdpt) {
    return PAGING_ERR_HUGE_PAGE;
  }

  uint64_t index = paging_pdpt_index(virtual_address);

  if (pdpt->entries[index] & PAGE_PRESENT) {
    return PAGING_ERR_ALREADY_MAPPED;
  }

  pdpt->entries[index] =
      paging_make_entry(physical_address, flags | PAGE_PRESENT | PAGE_HUGE);

  cpu_invlpg(virtual_address);

  return PAGING_OK;
}

paging_result_t paging_map(uint64_t virtual_address, uint64_t physical_address,
                           uint64_t flags) {
  page_table_t *pml4 = paging_current_pml4();

  return paging_map_4k_internal(pml4, virtual_address, physical_address, flags);
}

paging_result_t paging_map_2m(uint64_t virtual_address,
                              uint64_t physical_address, uint64_t flags) {
  page_table_t *pml4 = paging_current_pml4();
  return paging_map_2m_internal(pml4, virtual_address, physical_address, flags);
}

paging_result_t paging_map_1g(uint64_t virtual_address,
                              uint64_t physical_address, uint64_t flags) {
  page_table_t *pml4 = paging_current_pml4();
  return paging_map_1g_internal(pml4, virtual_address, physical_address, flags);
}

paging_result_t paging_map_range(uint64_t virtual_address,
                                 uint64_t physical_address, uint64_t size,
                                 uint64_t flags) {

  page_table_t *pml4 = paging_current_pml4();

  if (!paging_range_valid(virtual_address, size)) {
    return PAGING_ERR_INVALID_RANGE;
  }

  if (!paging_physical_range_valid(physical_address, size)) {
    return PAGING_ERR_INVALID_RANGE;
  }

  if ((virtual_address & (PAGE_SIZE - 1)) != 0 ||
      (physical_address & (PAGE_SIZE - 1)) != 0 ||
      (size & (PAGE_SIZE - 1)) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  uint64_t remaining = size;

  while (remaining != 0) {
    paging_result_t result;

    if (supports_1g_pages && remaining >= PAGE_1G_SIZE &&
        (virtual_address & (PAGE_1G_SIZE - 1)) == 0 &&
        (physical_address & (PAGE_1G_SIZE - 1)) == 0) {
      result = paging_map_1g_internal(pml4, virtual_address, physical_address,
                                      flags);

      if (result != PAGING_OK) {
        return result;
      }

      virtual_address += PAGE_1G_SIZE;
      physical_address += PAGE_1G_SIZE;
      remaining -= PAGE_1G_SIZE;
      continue;
    }

    if (remaining >= PAGE_2M_SIZE &&
        (virtual_address & (PAGE_2M_SIZE - 1)) == 0 &&
        (physical_address & (PAGE_2M_SIZE - 1)) == 0) {
      result = paging_map_2m_internal(pml4, virtual_address, physical_address,
                                      flags);

      if (result != PAGING_OK) {
        return result;
      }

      virtual_address += PAGE_2M_SIZE;
      physical_address += PAGE_2M_SIZE;
      remaining -= PAGE_2M_SIZE;
      continue;
    }

    result =
        paging_map_4k_internal(pml4, virtual_address, physical_address, flags);

    if (result != PAGING_OK) {
      return result;
    }

    virtual_address += PAGE_SIZE;
    physical_address += PAGE_SIZE;
    remaining -= PAGE_SIZE;
  }

  return PAGING_OK;
}

paging_result_t paging_translate(uint64_t virtual_address,
                                 uint64_t *physical_address,
                                 paging_page_size_t *page_size) {
  if (!physical_address) {
    return PAGING_ERR_INVALID_ADDRESS;
  }

  page_table_entry_t *entry;
  paging_page_size_t size;

  paging_result_t result =
      paging_lookup(paging_current_pml4(), virtual_address, &entry, &size);

  if (result != PAGING_OK) {
    return result;
  }

  uint64_t base = paging_entry_address(*entry);
  uint64_t offset;

  switch (size) {
  case PAGING_PAGE_1G:
    offset = virtual_address & (PAGE_1G_SIZE - 1);
    break;

  case PAGING_PAGE_2M:
    offset = virtual_address & (PAGE_2M_SIZE - 1);
    break;

  case PAGING_PAGE_4K:
    offset = virtual_address & (PAGE_SIZE - 1);
    break;

  default:
    return PAGING_ERR_INVALID_ADDRESS;
  }

  *physical_address = base + offset;

  if (page_size) {
    *page_size = size;
  }

  return PAGING_OK;
}

paging_result_t paging_translate_in_address_space(
    uint64_t page_table_root, uint64_t virtual_address,
    uint64_t *physical_address, paging_page_size_t *page_size) {

  if (physical_address == 0) {
    return PAGING_ERR_INVALID_ADDRESS;
  }

  if ((page_table_root & (PAGE_SIZE - 1)) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  page_table_t *pml4 =
      (page_table_t *)paging_physical_to_virtual(page_table_root);

  page_table_entry_t *entry;
  paging_page_size_t size;

  paging_result_t result = paging_lookup(pml4, virtual_address, &entry, &size);

  if (result != PAGING_OK) {
    return result;
  }

  uint64_t base = paging_entry_address(*entry);
  uint64_t offset;

  switch (size) {
  case PAGING_PAGE_1G:
    offset = virtual_address & (PAGE_1G_SIZE - 1);
    break;

  case PAGING_PAGE_2M:
    offset = virtual_address & (PAGE_2M_SIZE - 1);
    break;

  case PAGING_PAGE_4K:
    offset = virtual_address & (PAGE_SIZE - 1);
    break;

  default:
    return PAGING_ERR_INVALID_ADDRESS;
  }

  *physical_address = base + offset;

  if (page_size) {
    *page_size = size;
  }

  return PAGING_OK;
}

paging_result_t paging_protect(uint64_t virtual_address, uint64_t flags) {
  if ((virtual_address & (PAGE_SIZE - 1)) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  if (flags & PAGE_HUGE) {
    return PAGING_ERR_INVALID_FLAGS;
  }

  paging_result_t result = paging_validate_flags(flags);

  if (result != PAGING_OK) {
    return result;
  }

  page_table_entry_t *entry;
  paging_page_size_t page_size;

  result =
      paging_lookup(paging_current_pml4(), virtual_address, &entry, &page_size);

  if (result != PAGING_OK) {
    return result;
  }

  if (page_size != PAGING_PAGE_4K) {
    return PAGING_ERR_HUGE_PAGE;
  }

  uint64_t physical = paging_entry_address(*entry);

  *entry = paging_make_entry(physical, flags | PAGE_PRESENT);

  cpu_invlpg(virtual_address);

  return PAGING_OK;
}

paging_result_t paging_unmap(uint64_t virtual_address) {
  if ((virtual_address & (PAGE_SIZE - 1)) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  page_table_entry_t *entry;
  paging_page_size_t page_size;

  paging_result_t result =
      paging_lookup(paging_current_pml4(), virtual_address, &entry, &page_size);

  if (result != PAGING_OK) {
    return result;
  }

  if (page_size != PAGING_PAGE_4K) {
    return PAGING_ERR_HUGE_PAGE;
  }

  *entry = 0;

  cpu_invlpg(virtual_address);

  return PAGING_OK;
}

paging_result_t paging_unmap_2m(uint64_t virtual_address) {
  if ((virtual_address & (PAGE_2M_SIZE - 1)) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  page_table_entry_t *entry;
  paging_page_size_t page_size;

  paging_result_t result =
      paging_lookup(paging_current_pml4(), virtual_address, &entry, &page_size);

  if (result != PAGING_OK) {
    return result;
  }

  if (page_size != PAGING_PAGE_2M) {
    return PAGING_ERR_HUGE_PAGE;
  }

  *entry = 0;

  cpu_invlpg(virtual_address);

  return PAGING_OK;
}

paging_result_t paging_unmap_1g(uint64_t virtual_address) {
  if ((virtual_address & (PAGE_1G_SIZE - 1)) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  page_table_entry_t *entry;
  paging_page_size_t page_size;

  paging_result_t result =
      paging_lookup(paging_current_pml4(), virtual_address, &entry, &page_size);

  if (result != PAGING_OK) {
    return result;
  }

  if (page_size != PAGING_PAGE_1G) {
    return PAGING_ERR_HUGE_PAGE;
  }

  *entry = 0;

  cpu_invlpg(virtual_address);

  return PAGING_OK;
}

static void paging_setup_direct_map(void) {
  uint64_t physical_end = physical_memory_highest_address();

  physical_end = align_up(physical_end);

  if (physical_end == UINT64_MAX) {
    panic("physical memory range overflow");
  }

  if (physical_end > PHYS_MAP_SIZE) {
    panic("physical memory exceeds direct-map range");
  }

  uint64_t physical = PAGE_1G_SIZE;

  while (physical < physical_end) {
    uint64_t remaining = physical_end - physical;
    uint64_t virtual_address = PHYS_MAP_BASE + physical;

    paging_result_t result;

    if (supports_1g_pages && remaining >= PAGE_1G_SIZE &&
        (physical & (PAGE_1G_SIZE - 1)) == 0) {
      result = paging_map_1g(virtual_address, physical, PAGE_WRITABLE);

      if (result != PAGING_OK) {
        panic("unable to extend 1 GiB direct map");
      }

      physical += PAGE_1G_SIZE;
      continue;
    }

    if (remaining >= PAGE_2M_SIZE && (physical & (PAGE_2M_SIZE - 1)) == 0) {
      result = paging_map_2m(virtual_address, physical, PAGE_WRITABLE);

      if (result != PAGING_OK) {
        panic("unable to extend 2 MiB direct map");
      }

      physical += PAGE_2M_SIZE;
      continue;
    }

    result = paging_map(virtual_address, physical, PAGE_WRITABLE);

    if (result != PAGING_OK) {
      panic("unable to extend 4 KiB direct map");
    }

    physical += PAGE_SIZE;
  }
}

void paging_dump(uint64_t virtual_address) {
  page_table_t *pml4 = paging_current_pml4();

  uint64_t pml4e = pml4->entries[paging_pml4_index(virtual_address)];

  terminal_write("paging: virtual=0x");
  terminal_write_uint(virtual_address);
  terminal_write("\n");

  if (!(pml4e & PAGE_PRESENT)) {
    terminal_write("  not mapped at PML4\n");
    return;
  }

  page_table_t *pdpt =
      (page_table_t *)paging_physical_to_virtual(paging_entry_address(pml4e));

  uint64_t pdpte = pdpt->entries[paging_pdpt_index(virtual_address)];

  if (!(pdpte & PAGE_PRESENT)) {
    terminal_write("  not mapped at PDPT\n");
    return;
  }

  if (pdpte & PAGE_HUGE) {
    terminal_write("  mapping: 1 GiB\n");
    terminal_write("  physical=0x");
    terminal_write_uint(paging_entry_address(pdpte));
    terminal_write("\n");
    return;
  }

  page_table_t *pd =
      (page_table_t *)paging_physical_to_virtual(paging_entry_address(pdpte));

  uint64_t pde = pd->entries[paging_pd_index(virtual_address)];

  if (!(pde & PAGE_PRESENT)) {
    terminal_write("  not mapped at PD\n");
    return;
  }

  if (pde & PAGE_HUGE) {
    terminal_write("  mapping: 2 MiB\n");
    terminal_write("  physical=0x");
    terminal_write_uint(paging_entry_address(pde));
    terminal_write("\n");
    return;
  }

  page_table_t *pt =
      (page_table_t *)paging_physical_to_virtual(paging_entry_address(pde));

  uint64_t pte = pt->entries[paging_pt_index(virtual_address)];

  if (!(pte & PAGE_PRESENT)) {
    terminal_write("  not mapped at PT\n");
    return;
  }

  terminal_write("  mapping: 4 KiB\n");
  terminal_write("  physical=0x");
  terminal_write_uint(paging_entry_address(pte));
  terminal_write("\n");
}

paging_result_t paging_map_in_address_space(uint64_t page_table_root,
                                            uint64_t virtual_address,
                                            uint64_t physical_address,
                                            uint64_t flags) {

  if ((page_table_root & (PAGE_SIZE - 1)) != 0) {
    return PAGING_ERR_INVALID_ALIGNMENT;
  }

  page_table_t *pml4 =
      (page_table_t *)paging_physical_to_virtual(page_table_root);

  return paging_map_4k_internal(pml4, virtual_address, physical_address, flags);
}

void paging_init(void) {
  early_table_pool_used = 0;
  direct_map_ready = 0;

  paging_detect_cpu_features();
  paging_enable_nxe();

  paging_setup_direct_map();

  direct_map_ready = 1;
}
