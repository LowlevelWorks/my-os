#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_INFO_FLAG_CMDLINE (1 << 2)
#define MULTIBOOT_INFO_FLAG_MODS (1 << 3)
#define MULTIBOOT_INFO_FLAG_MEMORY_MAP (1 << 6)

#define MULTIBOOT_MEMORY_AVAILABLE 1

typedef struct {
  uint32_t flags;

  uint32_t mem_lower;
  uint32_t mem_upper;

  uint32_t boot_device;
  uint32_t cmdline;

  uint32_t mods_count;
  uint32_t mods_addr;

  uint32_t syms[4];

  uint32_t mmap_length;
  uint32_t mmap_addr;
} __attribute__((packed)) multiboot_info_t;

typedef struct {
  uint32_t size;

  uint64_t addr;
  uint64_t length;

  uint32_t type;
} __attribute((packed)) multiboot_memory_map_entry_t;

typedef struct {
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t string;
  uint32_t reserved;
} multiboot_module_t;

#endif
