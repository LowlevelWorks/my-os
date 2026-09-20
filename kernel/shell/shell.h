#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

void shell_init(void);
void shell_print_prompt(void);
void shell_handle_key(uint16_t key);

#endif
