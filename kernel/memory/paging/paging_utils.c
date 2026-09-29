#include "paging_utils.h"

#include "arch/x86_64/cpu.h"

#include <stdint.h>

#define CPUID_EXTENDED_MAX 0x80000000U
#define CPUID_EXTENDED_FEATURES_1 0x80000001U

#define CPUID_1G_PAGE_BIT (1U << 26)
#define CPUID_NX_BIT (1U << 20)

#define EFER_MSR 0xC0000080U
#define EFER_NXE (1ULL << 11)

static int supports_1g_pages;

static int paging_cpu_supports_cpuid_80000001(void) {
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;

  cpu_cpuid(CPUID_EXTENDED_MAX, &eax, &ebx, &ecx, &edx);

  return eax >= CPUID_EXTENDED_FEATURES_1;
}

void paging_detect_cpu_features(void) {
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;

  supports_1g_pages = 0;

  if (!paging_cpu_supports_cpuid_80000001()) {
    return;
  }

  cpu_cpuid(CPUID_EXTENDED_FEATURES_1, &eax, &ebx, &ecx, &edx);

  supports_1g_pages = (edx & CPUID_1G_PAGE_BIT) != 0;
}

int paging_cpu_supports_1g_pages(void) { return supports_1g_pages; }

void paging_enable_nxe(void) {
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;

  if (!paging_cpu_supports_cpuid_80000001()) {
    return;
  }

  cpu_cpuid(CPUID_EXTENDED_FEATURES_1, &eax, &ebx, &ecx, &edx);

  if (!(edx & CPUID_NX_BIT)) {
    return;
  }

  uint64_t efer = cpu_read_msr(EFER_MSR);

  efer |= EFER_NXE;

  cpu_write_msr(EFER_MSR, efer);
}

static int paging_add_overflow(uint64_t a, uint64_t b, uint64_t *result) {
  if (a > UINT64_MAX - b) {
    return 1;
  }

  *result = a + b;

  return 0;
}

int paging_is_canonical(uint64_t address) {
  uint64_t upper = address >> 48;

  if ((address >> 47) & 1ULL) {
    return upper == 0xFFFFULL;
  }

  return upper == 0;
}

int paging_range_valid(uint64_t address, uint64_t size) {
  uint64_t end;

  if (size == 0) {
    return 0;
  }

  if (paging_add_overflow(address, size - 1, &end)) {
    return 0;
  }

  return paging_is_canonical(address) && paging_is_canonical(end);
}

int paging_physical_range_valid(uint64_t address, uint64_t size) {
  uint64_t end;

  if (size == 0) {
    return 0;
  }

  if (paging_add_overflow(address, size - 1, &end)) {
    return 0;
  }

  return address <= PAGE_ADDRESS_MASK && end <= PAGE_ADDRESS_MASK;
}

paging_result_t paging_validate_flags(uint64_t flags) {
  const uint64_t allowed = PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_PWT |
                           PAGE_PCD | PAGE_HUGE | PAGE_GLOBAL | PAGE_NX;

  if (flags & ~allowed) {
    return PAGING_ERR_INVALID_FLAGS;
  }

  return PAGING_OK;
}

static uint64_t paging_index(uint64_t address, unsigned int shift) {
  return (address >> shift) & 0x1FFULL;
}

uint64_t paging_pml4_index(uint64_t address) {
  return paging_index(address, 39);
}

uint64_t paging_pdpt_index(uint64_t address) {
  return paging_index(address, 30);
}

uint64_t paging_pd_index(uint64_t address) { return paging_index(address, 21); }

uint64_t paging_pt_index(uint64_t address) { return paging_index(address, 12); }

uint64_t paging_entry_address(uint64_t entry) {
  return entry & PAGE_ADDRESS_MASK;
}

uint64_t paging_make_entry(uint64_t address, uint64_t flags) {
  return (address & PAGE_ADDRESS_MASK) | flags;
}

page_table_t *paging_current_pml4(void) {
  uint64_t physical = cpu_read_cr3() & PAGE_ADDRESS_MASK;

  return (page_table_t *)paging_physical_to_virtual(physical);
}
