#include "paging.h"

#include "arch/x86_64/cpu.h"
#include "drivers/terminal/terminal.h"
#include "lib/string.h"
#include "memory/buddy.h"
#include "memory/physical_memory.h"

#include <stdint.h>

extern uint64_t boot_temp_pd_phys(void);
extern uint64_t boot_temp_pt_phys(void);

#define EARLY_TABLE_POOL_PAGES 64

static uint8_t early_table_pool[EARLY_TABLE_POOL_PAGES][PAGE_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

static uint32_t early_table_pool_used = 0;

static int direct_map_ready = 0;

extern uint64_t kernel_load_start_phys(void);
extern char kernel_start[];

static inline uint64_t page_table_index(uint64_t address, unsigned int shift) {
  return (address >> shift) & 0x1FF;
}

static inline unsigned int pml4_index(uint64_t address) {
  return page_table_index(address, 39);
}

static inline unsigned int pdpt_index(uint64_t address) {
  return page_table_index(address, 30);
}

static inline unsigned int pd_index(uint64_t address) {
  return page_table_index(address, 21);
}

static inline unsigned int pt_index(uint64_t address) {
  return page_table_index(address, 12);
}

static inline uint64_t make_page_table_entry(uint64_t address, uint64_t flags) {
  return (address & PAGE_ADDRESS_MASK) | flags;
}

static inline int page_table_entry_present(uint64_t entry) {
  return (entry & PAGE_PRESENT) != 0;
}

static inline int page_table_entry_huge(uint64_t entry) {
  return (entry & PAGE_HUGE) != 0;
}

static inline uint64_t page_table_entry_address(uint64_t entry) {
  return entry & PAGE_ADDRESS_MASK;
}

static inline page_table_t *page_table_virtual(uint64_t physical_address) {
  return (page_table_t *)physical_to_virtual(physical_address);
}

static void paging_panic(const char *message) {
  terminal_write("PANIC (paging): ");
  terminal_write(message);
  terminal_write("\n");

  for (;;) {
    __asm__ volatile("cli");
    __asm__ volatile("hlt");
  }
}

static uint64_t paging_read_cr3(void) {
  uint64_t value;

  __asm__ volatile("mov %%cr3, %0" : "=r"(value));

  return value;
}

static void paging_invalidate(uint64_t virtual_address) {
  cpu_invalidate_page(virtual_address);
}

static int paging_supports_1g_pages(void) {
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;

  __asm__ volatile("cpuid"
                   : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                   : "a"(0x80000000));

  if (eax < 0x80000001) {
    return 0;
  }

  __asm__ volatile("cpuid"
                   : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                   : "a"(0x80000001));

  return (edx & (1U << 26)) != 0;
}

static page_table_t *paging_current_pml4(void) {
  uint64_t cr3 = paging_read_cr3();

  return page_table_virtual(cr3 & PAGE_ADDRESS_MASK);
}

static uint64_t paging_alloc_table(void) {
  if (!direct_map_ready) {
    if (early_table_pool_used >= EARLY_TABLE_POOL_PAGES) {
      paging_panic("early page-table pool exhausted");
    }

    void *table_virtual = early_table_pool[early_table_pool_used];
    early_table_pool_used++;

    memory_set(table_virtual, 0, PAGE_SIZE);

    uint64_t kernel_vma = (uint64_t)(uintptr_t)kernel_start;
    uint64_t table_physical = (uint64_t)(uintptr_t)table_virtual - kernel_vma +
                              kernel_load_start_phys();

    return table_physical;
  }

  physical_frame_t frame = buddy_alloc(0);

  if (frame == INVALID_FRAME) {
    paging_panic("unable to allocate page table");
  }

  uint64_t physical_address = frame_to_address(frame);

  page_table_t *table = (page_table_t *)paging_map_temporary(physical_address);

  memory_set(table, 0, PAGE_SIZE);

  return physical_address;
}

void *paging_map_temporary(uint64_t physical_address) {
  uint64_t aligned_address = align_down(physical_address);
  uint64_t offset = physical_address - aligned_address;

  page_table_t *temp_pd = page_table_virtual(boot_temp_pd_phys());

  page_table_t *temp_pt = page_table_virtual(boot_temp_pt_phys());

  temp_pd->entries[pd_index(PAGING_TEMP_BASE)] =
      make_page_table_entry(boot_temp_pt_phys(), PAGE_PRESENT | PAGE_WRITABLE);

  temp_pt->entries[pt_index(PAGING_TEMP_BASE)] =
      make_page_table_entry(aligned_address, PAGE_PRESENT | PAGE_WRITABLE);

  paging_invalidate(PAGING_TEMP_BASE);

  return (void *)(uintptr_t)(PAGING_TEMP_BASE + offset);
}

static page_table_t *paging_get_table(page_table_entry_t *entry,
                                      uint64_t flags) {
  if (!page_table_entry_present(*entry)) {
    uint64_t table_physical = paging_alloc_table();

    uint64_t table_flags = PAGE_PRESENT | PAGE_WRITABLE;

    if (flags & PAGE_USER) {
      table_flags |= PAGE_USER;
    }

    *entry = make_page_table_entry(table_physical, table_flags);

    return page_table_virtual(table_physical);
  }

  if (page_table_entry_huge(*entry)) {
    paging_panic("page-table hierarchy contains huge page");
  }

  if (flags & PAGE_USER) {
    *entry |= PAGE_USER;
  }

  return page_table_virtual(page_table_entry_address(*entry));
}

static void paging_map_1g(uint64_t physical_address, uint64_t virtual_address) {
  page_table_t *pml4 = paging_current_pml4();

  page_table_t *pdpt = paging_get_table(
      &pml4->entries[pml4_index(virtual_address)], PAGE_WRITABLE);

  unsigned int index = pdpt_index(virtual_address);

  if (page_table_entry_present(pdpt->entries[index])) {
    paging_panic("attempted to overwrite existing 1 GiB mapping");
  }

  pdpt->entries[index] = make_page_table_entry(
      physical_address, PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE);
}

static void paging_map_2m(uint64_t physical_address, uint64_t virtual_address) {
  page_table_t *pml4 = paging_current_pml4();

  page_table_t *pdpt = paging_get_table(
      &pml4->entries[pml4_index(virtual_address)], PAGE_WRITABLE);

  page_table_t *pd = paging_get_table(
      &pdpt->entries[pdpt_index(virtual_address)], PAGE_WRITABLE);

  unsigned int index = pd_index(virtual_address);

  if (page_table_entry_present(pd->entries[index])) {
    paging_panic("attempted to overwrite existing 2 MiB mapping");
  }

  pd->entries[index] = make_page_table_entry(
      physical_address, PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE);
}

static void paging_map_4k(uint64_t physical_address, uint64_t virtual_address,
                          uint64_t flags) {
  page_table_t *pml4 = paging_current_pml4();

  page_table_t *pdpt =
      paging_get_table(&pml4->entries[pml4_index(virtual_address)], flags);

  page_table_t *pd =
      paging_get_table(&pdpt->entries[pdpt_index(virtual_address)], flags);

  page_table_t *pt =
      paging_get_table(&pd->entries[pd_index(virtual_address)], flags);

  unsigned int index = pt_index(virtual_address);

  if (page_table_entry_present(pt->entries[index])) {
    paging_panic("attempted to overwrite existing page");
  }

  pt->entries[index] =
      make_page_table_entry(physical_address, flags | PAGE_PRESENT);

  paging_invalidate(virtual_address);
}

static void paging_map_direct_1g(uint64_t physical_address) {
  uint64_t virtual_address = PHYS_MAP_BASE + physical_address;

  paging_map_1g(physical_address, virtual_address);
}

static void paging_map_direct_2m(uint64_t physical_address) {
  uint64_t virtual_address = PHYS_MAP_BASE + physical_address;

  paging_map_2m(physical_address, virtual_address);
}

static void paging_map_direct_4k(uint64_t physical_address) {
  uint64_t virtual_address = PHYS_MAP_BASE + physical_address;

  paging_map_4k(physical_address, virtual_address, PAGE_WRITABLE);
}

static void paging_setup_direct_map(void) {
  uint64_t physical_end = align_up(physical_memory_highest_address());

  if (physical_end == 0) {
    return;
  }

  /*
   * The bootstrap page tables already map physical
   * memory from 0 through 1 GiB.
   *
   * Keep that mapping and extend it from 1 GiB onward.
   */

  uint64_t physical_address = PAGE_1G_SIZE;

  if (paging_supports_1g_pages()) {
    while (physical_address + PAGE_1G_SIZE <= physical_end) {
      paging_map_direct_1g(physical_address);

      physical_address += PAGE_1G_SIZE;
    }
  }

  /*
   * Map the remaining complete 2 MiB regions.
   */
  while (physical_address + PAGE_2M_SIZE <= physical_end) {
    paging_map_direct_2m(physical_address);

    physical_address += PAGE_2M_SIZE;
  }

  /*
   * Map the final partial 2 MiB region with 4 KiB pages.
   */
  while (physical_address < physical_end) {
    paging_map_direct_4k(physical_address);

    physical_address += PAGE_SIZE;
  }
}

void paging_map(uint64_t virtual_address, uint64_t physical_address,
                uint64_t flags) {
  if ((virtual_address & (PAGE_SIZE - 1)) != 0) {
    paging_panic("virtual address is not page aligned");
  }

  if ((physical_address & (PAGE_SIZE - 1)) != 0) {
    paging_panic("physical address is not page aligned");
  }

  paging_map_4k(physical_address, virtual_address, flags);
}

void paging_unmap(uint64_t virtual_address) {
  if ((virtual_address & (PAGE_SIZE - 1)) != 0) {
    paging_panic("virtual address is not page aligned");
  }

  page_table_t *pml4 = paging_current_pml4();

  page_table_entry_t *pml4_entry = &pml4->entries[pml4_index(virtual_address)];

  if (!page_table_entry_present(*pml4_entry)) {
    return;
  }

  if (page_table_entry_huge(*pml4_entry)) {
    paging_panic("invalid PML4 huge page");
  }

  page_table_t *pdpt =
      page_table_virtual(page_table_entry_address(*pml4_entry));

  page_table_entry_t *pdpt_entry = &pdpt->entries[pdpt_index(virtual_address)];

  if (!page_table_entry_present(*pdpt_entry)) {
    return;
  }

  if (page_table_entry_huge(*pdpt_entry)) {
    paging_panic("cannot unmap 1 GiB page with 4 KiB unmap");
  }

  page_table_t *pd = page_table_virtual(page_table_entry_address(*pdpt_entry));

  page_table_entry_t *pd_entry = &pd->entries[pd_index(virtual_address)];

  if (!page_table_entry_present(*pd_entry)) {
    return;
  }

  if (page_table_entry_huge(*pd_entry)) {
    paging_panic("cannot unmap 2 MiB page with 4 KiB unmap");
  }

  page_table_t *pt = page_table_virtual(page_table_entry_address(*pd_entry));

  unsigned int index = pt_index(virtual_address);

  if (!page_table_entry_present(pt->entries[index])) {
    return;
  }

  pt->entries[index] = 0;

  paging_invalidate(virtual_address);
}

void paging_init(void) {
  terminal_write("Paging: initializing\n");

  if (!paging_supports_1g_pages()) {
    terminal_write("Paging: CPU does not support 1 GiB pages\n");
    terminal_write("Paging: using 2 MiB/4 KiB direct mappings\n");
  } else {
    terminal_write("Paging: CPU supports 1 GiB pages\n");
  }

  paging_setup_direct_map();
  direct_map_ready = 1;

  terminal_write("Paging: direct map initialized\n");

  void *mapped = paging_map_temporary(0x100000);

  terminal_write("Temporary mapping address: ");
  terminal_write_uint((uint64_t)(uintptr_t)mapped);
  terminal_write("\n");

  terminal_write("Paging: initialized\n");
}
