#ifndef PAGING_UTILS_H
#define PAGING_UTILS_H

#include "paging.h"
#include <stdint.h>

void paging_detect_cpu_features(void);

int paging_cpu_supports_1g_pages(void);

void paging_enable_nxe(void);

int paging_is_canonical(uint64_t address);

int paging_range_valid(uint64_t address, uint64_t size);

int paging_physical_range_valid(uint64_t address, uint64_t size);

paging_result_t paging_validate_flags(uint64_t flags);

uint64_t paging_pml4_index(uint64_t address);
uint64_t paging_pdpt_index(uint64_t address);
uint64_t paging_pd_index(uint64_t address);
uint64_t paging_pt_index(uint64_t address);

uint64_t paging_entry_address(uint64_t entry);
uint64_t paging_make_entry(uint64_t address, uint64_t flags);

page_table_t *paging_current_pml4(void);

#endif
