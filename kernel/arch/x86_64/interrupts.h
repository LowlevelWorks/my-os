#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include<stdint.h>

#define IDT_MAX_DESCRIPTORS 256

struct idt_entry {
  uint16_t offset_low;
  uint16_t selector;
  uint8_t ist;
  uint8_t attributes;
  uint16_t offset_middle;
  uint32_t offset_high;
  uint32_t reserved;
} __attribute__((packed));

struct idt_descriptor {
 uint16_t limit;
 uint64_t base;
} __attribute__((packed));


void idt_init(void);
void idt_set_gate(int vector, uint64_t handler, uint8_t attributes);
void load_idt(void);

#endif
