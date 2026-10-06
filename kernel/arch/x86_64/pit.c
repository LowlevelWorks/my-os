#include "pit.h"

#include "arch/x86_64/io.h"
#include "arch/x86_64/pic.h"

#include "panic/panic.h"

#include "process/scheduler/scheduler.h"
#include "process/sleep/sleep.h"

#define pit_irq_vector 32
#define pit_cmd_channel0_mode3 0x36

#define pit_channel0 0x40
#define pit_command 0x43

#define pit_base_frequency 1193182u

static volatile uint64_t ticks = 0;

static void pit_set_frequency(uint32_t frequency) {
  if (frequency == 0) {
    panic("pit: invalid frequency");
  }

  uint32_t divisor = pit_base_frequency / frequency;

  if (divisor == 0 || divisor > 0xffff) {
    panic("pit: invalid divisor");
  }

  outb(pit_command, pit_cmd_channel0_mode3);

  outb(pit_channel0, (uint8_t)(divisor & 0xff));

  outb(pit_channel0, (uint8_t)((divisor >> 8) & 0xff));
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

  if (frame->vector != pit_irq_vector) {
    panic("pit: invalid interrupt vector");
  }

  pit_handle_irq();

  sleep_tick(pit_ticks());

  pic_send_eoi(0);

  scheduler_tick(frame);
}
