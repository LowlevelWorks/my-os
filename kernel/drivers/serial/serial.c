#include "serial.h"
#include "io.h"
#include <stdint.h>

#define UART_BASE 0x3F8

#define REG_DATA 0x00
#define REG_IER 0x01
#define REG_FCR 0x02
#define REG_LCR 0x03
#define REG_MCR 0x04
#define REG_LSR 0x05

#define LSR_DR 0x01
#define LSR_THRE 0x20

#define LCR_8N1 0x03
#define LCR_DLAB 0x80
#define FCR_FIFO_ENABLE 0x07
#define IER_RX_AVAILABLE 0x01
#define BAUD_DIVISOR 1 /* 115200 baud */

#define SERIAL_RX_BUFFER_SIZE 256

/*
 * Single-producer (IRQ4 handler) / single-consumer (shell poll) ring. Head and
 * tail are volatile so the compiler reloads them across the IRQ boundary.
 */
static volatile uint8_t serial_rx_buffer[SERIAL_RX_BUFFER_SIZE];
static volatile unsigned int serial_rx_head;
static volatile unsigned int serial_rx_tail;

static inline uint8_t uart_get_reg(uint8_t reg) { return inb(UART_BASE + reg); }

static inline void uart_set_reg(uint8_t reg, uint8_t value) {
  outb(UART_BASE + reg, value);
}

static void serial_rx_push(uint8_t b) {
  unsigned int next = (serial_rx_head + 1) % SERIAL_RX_BUFFER_SIZE;
  if (next == serial_rx_tail) {
    return; /* ring full: drop the byte rather than clobber unread input */
  }
  serial_rx_buffer[serial_rx_head] = b;
  serial_rx_head = next;
}

int serial_init(void) {
  uart_set_reg(REG_IER, 0x00); /* mask every UART IRQ while we reconfigure */

  uart_set_reg(REG_LCR, LCR_DLAB);             /* expose divisor latch */
  uart_set_reg(REG_DATA, BAUD_DIVISOR & 0xFF); /* divisor low  */
  uart_set_reg(REG_IER, BAUD_DIVISOR >> 8);    /* divisor high */
  uart_set_reg(REG_LCR, LCR_8N1);              /* DLAB off, 8N1 */

  uart_set_reg(REG_FCR, FCR_FIFO_ENABLE);

  /* Loopback self-test */
  uart_set_reg(REG_MCR, 0x1E);
  uart_set_reg(REG_DATA, 0xAE);
  if (uart_get_reg(REG_DATA) != 0xAE)
    return -1;

  /* DTR, RTS, OUT2. OUT2 gates IRQ4 onto the PIC on PC-compatible UARTs. */
  uart_set_reg(REG_MCR, 0x0F);

  serial_rx_head = 0;
  serial_rx_tail = 0;

  /* Raise IRQ4 whenever a byte lands in the receive buffer. */
  uart_set_reg(REG_IER, IER_RX_AVAILABLE);
  return 0;
}

static void serial_raw_putc(char c) {
  while ((uart_get_reg(REG_LSR) & LSR_THRE) == 0) {
  }
  uart_set_reg(REG_DATA, (uint8_t)c);
}

void serial_putc(char c) {
  if (c == '\n')
    serial_raw_putc('\r');
  serial_raw_putc(c);
}

/*
 * IRQ4 handler body. Called from the asm stub which handles register save and
 * EOI. Drains everything the UART has queued so a single IRQ clears the FIFO.
 */
void serial_handler(void) {
  while ((uart_get_reg(REG_LSR) & LSR_DR) != 0) {
    uint8_t b = uart_get_reg(REG_DATA);
    if (b == '\r')
      b = '\n';
    serial_rx_push(b);
  }
}

int serial_try_getc(uint8_t *c) {
  if (serial_rx_head == serial_rx_tail)
    return 0;

  *c = serial_rx_buffer[serial_rx_tail];
  serial_rx_tail = (serial_rx_tail + 1) % SERIAL_RX_BUFFER_SIZE;
  return 1;
}
