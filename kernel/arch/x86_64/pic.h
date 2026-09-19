#ifndef PIC_H
#define PIC_H

void pic_initialize(void);
void pic_send_eoi(uint8_t irq);
void pic_unmask_irq(uint8_t irq);
void pic_disable(void);
void pic_set_apic_mode(int enabled);

#endif

