#include "io.h"
#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21

#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01

#define ICW4_8086 0x01

static int apic_mode = 0;

void pic_set_apic_mode(int enabled) { apic_mode = enabled; }

void pic_disable(void) {
  /* Mask every legacy IRQ line on both PICs. */
  outb(PIC1_DATA, 0xFF);
  outb(PIC2_DATA, 0xFF);
}

void pic_initialize(void) {
  // Start initialization */
  outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

  // Set Vector offsets
  outb(PIC1_DATA, 32);
  outb(PIC2_DATA, 40);

  // Tell master PIC that slave is connected to IRQ2
  outb(PIC1_DATA, 0x04);

  // Tell slave PIC its cascade identity
  outb(PIC2_DATA, 0x02);

  // 8086/88 mode
  outb(PIC1_DATA, ICW4_8086);
  outb(PIC2_DATA, ICW4_8086);

  outb(PIC1_DATA, 0xFF); // mask everything...
  outb(PIC2_DATA,
       0xFF); // ...and let the caller unmask only what has a real handler
}

void pic_unmask_irq(uint8_t irq) {
  uint16_t port;

  if (irq < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
  }

  uint8_t mask = inb(port);
  mask &= ~(1 << (irq % 8));
  outb(port, mask);
}

void pic_send_eoi(uint8_t irq) {
  if (apic_mode) {
    // lapic_send_eoi();
    return;
  }

  if (irq >= 8) {
    outb(PIC2_COMMAND, PIC_EOI);
  }
  outb(PIC1_COMMAND, PIC_EOI);
}
