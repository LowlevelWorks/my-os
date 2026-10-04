#include "gdt.h"

#define GDT_CODE_SELECTOR 0x08
#define GDT_DATA_SELECTOR 0x10

static uint64_t gdt[3];

static gdt_descriptor_t gdt_descriptor;

extern void gdt_load(const gdt_descriptor_t *descriptor);

static uint64_t gdt_make_code_descriptor(void) { return 0x00AF9A000000FFFFULL; }

static uint64_t gdt_make_data_descriptor(void) { return 0x00AF92000000FFFFULL; }

void gdt_init(void) {
  gdt[0] = 0;
  gdt[1] = gdt_make_code_descriptor();
  gdt[2] = gdt_make_data_descriptor();

  gdt_descriptor.limit = sizeof(gdt) - 1;
  gdt_descriptor.base = (uint64_t)gdt;

  gdt_load(&gdt_descriptor);
}
