#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

int serial_init(void);
void serial_putc(char c);
int serial_try_getc(uint8_t *c);

#endif
