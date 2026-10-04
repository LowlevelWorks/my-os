#ifndef ARCH_X86_64_PIC_H
#define ARCH_X86_64_PIC_H

#include <stdint.h>

void pic_initialize(void);

void pic_set_apic_mode(int enabled);

void pic_unmask_irq(uint8_t irq);

void pic_mask_irq(uint8_t irq);

void pic_send_eoi(uint8_t irq);

#endif
