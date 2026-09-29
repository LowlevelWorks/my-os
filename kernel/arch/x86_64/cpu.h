#ifndef CPU_H
#define CPU_H

#include <stdint.h>

void cpu_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
               uint32_t *edx);

uint64_t cpu_read_msr(uint32_t msr);
void cpu_write_msr(uint32_t msr, uint64_t value);

uint64_t cpu_read_cr3(void);
void cpu_write_cr3(uint64_t value);

void cpu_invlpg(uint64_t address);

#endif
