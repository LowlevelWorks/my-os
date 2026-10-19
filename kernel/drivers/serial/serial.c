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
#define BAUD_DIVISOR 1 /* 115200 baud */

static inline uint8_t uart_get_reg(uint8_t reg) { return inb(UART_BASE + reg); }

static inline void uart_set_reg(uint8_t reg, uint8_t value) {
  outb(UART_BASE + reg, value);
}

int serial_init(void) {
  uart_set_reg(REG_IER, 0x00); /* polled: no interrupts */

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

  uart_set_reg(REG_MCR, 0x0F); /* DTR, RTS, OUT2 */
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

int serial_try_getc(uint8_t *c) {
  if ((uart_get_reg(REG_LSR) & LSR_DR) == 0)
    return 0;

  *c = uart_get_reg(REG_DATA);
  if (*c == '\r')
    *c = '\n';
  return 1;
}
