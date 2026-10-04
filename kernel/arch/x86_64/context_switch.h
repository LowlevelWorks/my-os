#ifndef ARCH_X86_64_CONTEXT_SWITCH_H
#define ARCH_X86_64_CONTEXT_SWITCH_H

#include <stdint.h>

void context_switch(uint64_t *old_saved_rsp, uint64_t new_saved_rsp,
                    uint64_t new_cr3);

__attribute__((noreturn)) void resume_preempted(uint64_t interrupt_frame,
                                                uint64_t new_cr3);

#endif
