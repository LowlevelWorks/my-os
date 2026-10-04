#include "pit.h"

#include "arch/x86_64/io.h"
#include "arch/x86_64/pic.h"

#include "panic/panic.h"

#include "process/scheduler.h"

#define PIT_IRQ_VECTOR 32
#define PIT_CMD_CHANNEL0_MODE3 0x36

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43

#define PIT_BASE_FREQUENCY 1193182U

static volatile uint64_t ticks = 0;

static void pit_set_frequency(uint32_t frequency) {
  if (frequency == 0) {
    panic("pit: invalid frequency");
  }

  uint32_t divisor = (PIT_BASE_FREQUENCY + frequency / 2) / frequency;

  if (divisor == 0 || divisor > 0xFFFF) {
    panic("pit: invalid divisor");
  }

  outb(PIT_COMMAND, PIT_CMD_CHANNEL0_MODE3);

  outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));

  outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

void pit_init(uint32_t frequency) {
  ticks = 0;

  pit_set_frequency(frequency);
}

uint64_t pit_ticks(void) { return ticks; }

void pit_handle_irq(void) { ticks++; }

void pit_handle_irq_frame(interrupt_frame_t *frame) {
  if (frame == 0) {
    panic("pit: null interrupt frame");
  }

  if (frame->vector != PIT_IRQ_VECTOR) {
    panic("pit: invalid interrupt vector");
  }

  pit_handle_irq();

  scheduler_tick(frame);

  pic_send_eoi(0);
}
