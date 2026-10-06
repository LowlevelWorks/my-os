#include "address_space.h"

#include "arch/x86_64/cpu.h"
#include "memory/buddy/buddy.h"
#include "memory/frame/frame.h"
#include "memory/paging/paging.h"
#include "panic/panic.h"

#define KERNEL_PML4_INDEX 511
#define PHYS_MAP_PML4_INDEX 256

static uint64_t current_page_table_root(void) {
  return cpu_read_cr3() & PAGE_ADDRESS_MASK;
}

address_space_t *address_space_create_kernel(void) {
  physical_frame_t address_space_frame = frame_alloc();

  if (address_space_frame == INVALID_FRAME) {
    panic("address_space_create_kernel: "
          "unable to allocate address space");
  }

  uint64_t address_space_physical = frame_to_address(address_space_frame);

  address_space_t *address_space =
      (address_space_t *)paging_physical_to_virtual(address_space_physical);

  physical_frame_t pml4_frame = buddy_alloc(0);

  if (pml4_frame == INVALID_FRAME) {
    frame_free(address_space_frame);

    panic("address_space_create_kernel: "
          "unable to allocate PML4");
  }

  uint64_t pml4_physical = frame_to_address(pml4_frame);

  page_table_t *new_pml4 =
      (page_table_t *)paging_physical_to_virtual(pml4_physical);

  for (uint64_t i = 0; i < PAGING_ENTRIES; i++) {
    new_pml4->entries[i] = 0;
  }

  page_table_t *current_pml4 =
      (page_table_t *)paging_physical_to_virtual(current_page_table_root());

  new_pml4->entries[PHYS_MAP_PML4_INDEX] =
      current_pml4->entries[PHYS_MAP_PML4_INDEX];

  new_pml4->entries[KERNEL_PML4_INDEX] =
      current_pml4->entries[KERNEL_PML4_INDEX];

  address_space->page_table_root = pml4_physical;

  return address_space;
}

void address_space_switch(address_space_t *address_space) {
  if (address_space == 0) {
    panic("address_space_switch: null address space");
  }

  cpu_write_cr3(address_space->page_table_root);
}

paging_result_t address_space_map(address_space_t *address_space,
                                  uint64_t virtual_address,
                                  uint64_t physical_address, uint64_t flags) {
  if (address_space == 0) {
    panic("address_space_map: null address space");
  }

  return paging_map_in_address_space(address_space->page_table_root,
                                     virtual_address, physical_address, flags);
}
void address_space_destroy(address_space_t *address_space) {
  if (address_space == 0) {
    panic("address_space_destroy: null address space");
  }

  uint64_t pml4_physical = address_space->page_table_root;

  if (pml4_physical == 0) {
    panic("address_space_destroy: invalid page table root");
  }

  physical_frame_t pml4_frame = address_to_frame(pml4_physical);

  physical_frame_t address_space_frame =
      address_to_frame(paging_virtual_to_physical((uint64_t)address_space));

  frame_free(pml4_frame);
  frame_free(address_space_frame);
}
