#ifndef STRING_H
#define STRING_H

#include <stdint.h>

int string_equals(const char *a, const char *b);
void string_copy(char *destination, const char *source);
uint64_t string_length(const char *s);
void *memory_set(void *destination, int value, uint64_t size);
#endif
