#ifndef ADDRESS_SPACE_H
#define ADDRESS_SPACE_H

#include "memory/paging/paging.h"
#include <stdint.h>

typedef struct address_space address_space_t;

struct address_space {
  uint64_t page_table_root;
};

void address_space_init(void);
address_space_t *address_space_create_kernel(void);
void address_space_switch(address_space_t *address_space);

paging_result_t address_space_map(address_space_t *address_space,
                                  uint64_t virtual_address,
                                  uint64_t physical_address, uint64_t flags);

void address_space_destroy(address_space_t *address_space);

#endif
