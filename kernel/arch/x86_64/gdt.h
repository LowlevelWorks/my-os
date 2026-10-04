#ifndef ARCH_X86_64_GDT_H
#define ARCH_X86_64_GDT_H

#include <stdint.h>

typedef struct {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed)) gdt_descriptor_t;

void gdt_init(void);

#endif
