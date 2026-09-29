#include "cpu.h"

void cpu_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx,
               uint32_t *edx) {
  __asm__ volatile("cpuid"
                   : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                   : "a"(leaf));
}

uint64_t cpu_read_msr(uint32_t msr) {
  uint32_t low;
  uint32_t high;

  __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));

  return ((uint64_t)high << 32) | low;
}

void cpu_write_msr(uint32_t msr, uint64_t value) {
  uint32_t low = (uint32_t)value;
  uint32_t high = (uint32_t)(value >> 32);

  __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

uint64_t cpu_read_cr3(void) {
  uint64_t value;

  __asm__ volatile("mov %%cr3, %0" : "=r"(value));

  return value;
}

void cpu_write_cr3(uint64_t value) {
  __asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

void cpu_invlpg(uint64_t address) {
  __asm__ volatile("invlpg (%0)" : : "r"(address) : "memory");
}
