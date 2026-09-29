#include "memblock.h"
#include "drivers/terminal/terminal.h"
#include "lib/string.h"
#include "memory/paging/paging.h"
#include "memory/physical_memory/physical_memory.h"
#include <stdint.h>

static memblock_reservation_t memory_reservations[MEMBLOCK_MAX_RESERVATIONS];
static uint32_t memory_reservation_count = 0;

extern uint64_t boot_load_start_phys(void);
extern uint64_t boot_load_end_phys(void);

extern uint64_t kernel_load_start_phys(void);
extern uint64_t kernel_load_end_phys(void);

static void memblock_fatal(const char *message) {
  terminal_write("PANIC (memblock): ");
  terminal_write(message);
  terminal_write("\n");

  for (;;) {
    __asm__ volatile("cli");
    __asm__ volatile("hlt");
  }
}

static int is_power_of_two(uint64_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

void memblock_reserve(uint64_t base, uint64_t length) {
  if (length == 0)
    return;

  if (length > UINT64_MAX - base) {
    memblock_fatal("reservation range overflow");
  }

  if (memory_reservation_count >= MEMBLOCK_MAX_RESERVATIONS) {
    memblock_fatal("reservation table full");
  }

  memory_reservations[memory_reservation_count].base = base;
  memory_reservations[memory_reservation_count].length = length;

  memory_reservation_count++;
}

static uint64_t scan_reservation_overlap(uint64_t base, uint64_t end) {
  uint64_t reservation_end_max = MEMBLOCK_INVALID_ADDRESS;

  for (uint32_t i = 0; i < memory_reservation_count; i++) {
    uint64_t reservation_start = memory_reservations[i].base;
    uint64_t reservation_end =
        reservation_start + memory_reservations[i].length;

    if (base < reservation_end && end > reservation_start) {
      if (reservation_end_max == MEMBLOCK_INVALID_ADDRESS ||
          reservation_end > reservation_end_max) {
        reservation_end_max = reservation_end;
      }
    }
  }

  return reservation_end_max;
}

int memblock_range_is_reserved(uint64_t base, uint64_t length) {
  if (length == 0)
    return 0;

  if (length > UINT64_MAX - base)
    return 1;

  return scan_reservation_overlap(base, base + length) !=
         MEMBLOCK_INVALID_ADDRESS;
}

uint64_t memblock_get_reservation_end(uint64_t base, uint64_t length) {
  if (length == 0)
    return MEMBLOCK_INVALID_ADDRESS;

  if (length > UINT64_MAX - base)
    return MEMBLOCK_INVALID_ADDRESS;

  return scan_reservation_overlap(base, base + length);
}

void memblock_reserve_boot(void) {
  uint64_t boot_start = align_down(boot_load_start_phys());

  uint64_t boot_end = align_up(boot_load_end_phys());

  memblock_reserve(boot_start, boot_end - boot_start);
}

void memblock_reserve_kernel(void) {
  uint64_t kernel_start_address = align_down(kernel_load_start_phys());

  uint64_t kernel_end_address = align_up(kernel_load_end_phys());

  memblock_reserve(kernel_start_address,
                   kernel_end_address - kernel_start_address);
}

void memblock_reserve_multiboot(const multiboot_info_t *multiboot_info) {
  if (multiboot_info == 0) {
    memblock_fatal("NULL multiboot info");
  }

  uint64_t info_address = (uint64_t)(uintptr_t)multiboot_info;

  memblock_reserve(info_address, sizeof(multiboot_info_t));

  if (multiboot_info->flags & MULTIBOOT_INFO_FLAG_MEMORY_MAP) {
    memblock_reserve(multiboot_info->mmap_addr, multiboot_info->mmap_length);
  }

  if ((multiboot_info->flags & MULTIBOOT_INFO_FLAG_CMDLINE) &&
      multiboot_info->cmdline != 0) {
    const char *cmdline = (const char *)(uintptr_t)multiboot_info->cmdline;

    memblock_reserve(multiboot_info->cmdline, string_length(cmdline) + 1);
  }

  if ((multiboot_info->flags & MULTIBOOT_INFO_FLAG_MODS) &&
      multiboot_info->mods_count != 0) {
    memblock_reserve(multiboot_info->mods_addr,
                     (uint64_t)multiboot_info->mods_count *
                         sizeof(multiboot_module_t));

    const multiboot_module_t *mods =
        (const multiboot_module_t *)(uintptr_t)multiboot_info->mods_addr;

    for (uint32_t i = 0; i < multiboot_info->mods_count; i++) {
      uint64_t start = mods[i].mod_start;
      uint64_t end = mods[i].mod_end;

      if (end > start) {
        memblock_reserve(start, end - start);
      }

      uint32_t string_address = mods[i].string;

      if (string_address != 0) {
        const char *name = (const char *)(uintptr_t)string_address;

        memblock_reserve(string_address, string_length(name) + 1);
      }
    }
  }
}

uint64_t memblock_alloc(uint64_t size, uint64_t alignment) {
  if (size == 0 || alignment == 0) {
    return MEMBLOCK_INVALID_ADDRESS;
  }

  if (!is_power_of_two(alignment)) {
    memblock_fatal("allocation alignment is not a power of two");
  }

  for (uint32_t i = 0; i < physical_memory_region_count(); i++) {
    const physical_memory_region_t *region = physical_memory_get_region(i);

    if (region == 0) {
      continue;
    }

    if (region->type != MULTIBOOT_MEMORY_AVAILABLE) {
      continue;
    }

    uint64_t address = align_up(region->base);

    if (alignment > 1) {
      address = (address + alignment - 1) & ~(alignment - 1);
    }

    uint64_t region_end = region->base + region->length;

    while (address < region_end && size <= region_end - address) {
      uint64_t reservation_end = memblock_get_reservation_end(address, size);

      if (reservation_end == MEMBLOCK_INVALID_ADDRESS) {
        memblock_reserve(address, size);
        return address;
      }

      address = align_up(reservation_end);

      if (alignment > 1) {
        address = (address + alignment - 1) & ~(alignment - 1);
      }
    }
  }

  return MEMBLOCK_INVALID_ADDRESS;
}

void memblock_init(void) { memory_reservation_count = 0; }
