#include "string.h"

int string_equals(const char *a, const char *b) {
  while (*a && *b) {
    if (*a != *b) {
      return 0;
    }
    a++;
    b++;
  }
  return *a == *b;
}

void string_copy(char *destination, const char *source) {
  while (*source) {
    *destination = *source;
    destination++;
    source++;
  }
  *destination = '\0';
}
