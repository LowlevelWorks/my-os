# Serial Driver

The serial driver talks to the PC's UART (Universal Asynchronous Receiver-Transmitter) hardware through I/O ports.

It gives the kernel a simple, polled serial console:

- `serial_init()` — initialize the UART
- `serial_putc()` — write one character
- `serial_try_getc()` — try to read one character without blocking

The serial port is commonly used as the kernel's debug console: QEMU can connect it to the host terminal with `-serial stdio`, so kernel output shows up outside the VM.

```text
Kernel
   ↓
serial_putc()
   ↓
UART at I/O port 0x3F8 (COM1)
   ↓
TX pin
   ↓
Host terminal (QEMU -serial stdio)
```

---

## 1. UART Base Address and Registers

We use COM1, the conventional first serial port on PCs:

```c
#define UART_BASE 0x3F8
```

The UART exposes several registers at consecutive offsets from the base port:

| Register | Offset | Purpose |
|----------|--------|---------|
| DATA     | 0x00   | Transmit / receive data |
| IER      | 0x01   | Interrupt enable |
| FCR      | 0x02   | FIFO control |
| LCR      | 0x03   | Line control |
| MCR      | 0x04   | Modem control |
| LSR      | 0x05   | Line status |

The driver reads and writes these registers with port I/O:

```c
static inline uint8_t uart_get_reg(uint8_t reg) {
  return inb(UART_BASE + reg);
}

static inline void uart_set_reg(uint8_t reg, uint8_t value) {
  outb(UART_BASE + reg, value);
}
```

### Why port 0x3F8?

`0x3F8` is COM1 on standard PC hardware. QEMU's `-serial stdio` connects this port to the host terminal, which is why serial output shows up on the host without any extra configuration.

---

## 2. Initialization — Baud Rate and Line Format

`serial_init()` puts the UART into a known state before the kernel uses it:

```c
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
```

### Baud rate

The UART generates a baud rate by dividing its input clock with a 16-bit divisor. To program the divisor:

1. Set the **DLAB** bit in the Line Control Register (`0x80`). This exposes the divisor latch registers.
2. Write the low byte of the divisor to the DATA register and the high byte to the IER register.
3. Clear DLAB so the data register returns to normal transmit/receive operation.

```c
#define LCR_DLAB 0x80
#define BAUD_DIVISOR 1 /* 115200 baud */
```

Divisor `1` produces 115200 baud.

### Line format — 8N1

Once the baud rate is programmed, LCR is set to `0x03`:

- 8 data bits
- No parity
- 1 stop bit

This is the common **8N1** format:

```c
#define LCR_8N1 0x03
```

### FIFO

The UART's transmit and receive FIFOs are enabled:

```c
#define FCR_FIFO_ENABLE 0x07
```

### Interrupts are disabled

```c
uart_set_reg(REG_IER, 0x00); /* polled: no interrupts */
```

The driver does not use UART interrupts. It polls status bits instead, which keeps the serial driver simple and safe to use very early in boot, before the interrupt system is fully set up.

---

## 3. Polled Output — `serial_putc()`

`serial_putc()` writes one character to the serial port:

```c
void serial_putc(char c) {
  if (c == '\n')
    serial_raw_putc('\r');
  serial_raw_putc(c);
}
```

Before sending each byte, the driver polls the **Line Status Register (LSR)** until the transmitter is ready:

```c
#define LSR_THRE 0x20

while ((uart_get_reg(REG_LSR) & LSR_THRE) == 0) {
}
```

**THRE = Transmitter Holding Register Empty.** When this bit is set, the UART can accept the next byte. The byte is then written to the DATA register:

```c
uart_set_reg(REG_DATA, (uint8_t)c);
```

### New lines become carriage return + new line

Serial terminals expect `\r\n` to move to the start of the next line. The driver converts:

```text
'\n' → '\r' '\n'
```

So kernel output such as `terminal_write("hello\n")` displays correctly on the serial console instead of just moving down without returning to the left margin.

---

## 4. Polled Input — `serial_try_getc()`

Input is polled the same way. The driver checks the **Data Ready (DR)** bit of LSR:

```c
#define LSR_DR 0x01

int serial_try_getc(uint8_t *c) {
  if ((uart_get_reg(REG_LSR) & LSR_DR) == 0)
    return 0;

  *c = uart_get_reg(REG_DATA);
  if (*c == '\r')
    *c = '\n';
  return 1;
}
```

- Returns `0` when no byte has arrived yet — the caller never blocks.
- Returns `1` and stores the received byte when data is available.
- Carriage return `\r` from the remote side is normalized to `\n`.

It is called "try" because it never waits: if nothing has arrived, the caller gets `0` immediately and can do other work.

---

## 5. Loopback Self-Test

`serial_init()` verifies the UART is working before returning success, using a loopback test:

1. Set MCR to `0x1E`, which enables loopback mode.
2. Write `0xAE` to the DATA register.
3. Read DATA back. If the value is not `0xAE`, initialization fails:

```c
uart_set_reg(REG_MCR, 0x1E);
uart_set_reg(REG_DATA, 0xAE);
if (uart_get_reg(REG_DATA) != 0xAE)
  return -1;
```

In loopback mode, the transmitter output is internally connected to the receiver input, so anything written can be read back immediately. This catches a missing or malfunctioning UART early.

Finally, MCR is set to `0x0F` (DTR, RTS, OUT2) for normal operation:

```c
uart_set_reg(REG_MCR, 0x0F); /* DTR, RTS, OUT2 */
```

---

## 6. Driver Interface

The rest of the kernel uses three functions declared in `serial.h`:

```c
int serial_init(void);
void serial_putc(char c);
int serial_try_getc(uint8_t *c);
```

For example:

```c
serial_putc('A');
```

```c
uint8_t c;
if (serial_try_getc(&c)) {
  // c contains a character from the serial port
}
```

The **terminal driver** mirrors every character written to the VGA screen onto the serial console:

```text
VGA screen  ←  terminal_putchar()
      ↑
terminal_emit()
      |
      v
serial_putc()
```

This means kernel messages appear both on the VGA text buffer and on the host terminal through QEMU's serial connection.

---

## 7. Overall Concept

The serial driver performs four main jobs:

```text
1. Initialize
   UART base port → baud divisor → 8N1 format → FIFO enabled

2. Transmit
   Poll LSR.THRE → write DATA register → '\n' becomes '\r\n'

3. Receive
   Poll LSR.DR → read DATA register → '\r' becomes '\n'

4. Test
   Loopback self-test during initialization
```

The key idea is:

> **The serial driver converts kernel characters into UART bytes (and vice versa) using polled port I/O, without interrupts.**

This gives the OS a simple, reliable console that works early in boot and across QEMU sessions.
