#ifndef CPU_CONTEXT_H
#define CPU_CONTEXT_H

#include <stdint.h>

typedef struct {
  uint64_t r15;
  uint64_t r14;
  uint64_t r13;
  uint64_t r12;
  uint64_t rbx;
  uint64_t rbp;
  uint64_t rip;
} cpu_context_t;

#endif
