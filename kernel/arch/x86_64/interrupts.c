#include<stdint.h>
#include "interrupts.h"


extern void invalid_opcode_handler(void);
extern void keyboard_handler(void);

static struct idt_entry idt[IDT_MAX_DESCRIPTORS];
static struct idt_descriptor idt_descriptor;


void idt_set_gate(int vector, uint64_t handler, uint8_t attributes) {
    // get the lowest 16 bit;
    idt[vector].offset_low = handler & 0xFFFF;
    //selector = 0x08 is the GDT kernel code segment;
    idt[vector].selector = 0x08;
    // not using interrupt table yet;
    idt[vector].ist = 0;
    // 0x8E = Present, kernel-level, 64-bit interrupt gate; 
    idt[vector].attributes = attributes;
    // Right shift 16 bits and gets the next 16 bits;
    idt[vector].offset_middle = (handler >> 16) & 0xFFFF;
    // Right shift 32 bits and gets the next 32 bits;
    idt[vector].offset_high = (handler >>  32) & 0xFFFFFFFF;
    // Those bytes are reserved and must be zero;
    idt[vector].reserved = 0;
}


void idt_init(void){
   idt_descriptor.limit = sizeof(idt) - 1;
   idt_descriptor.base = (uint64_t)idt;

 /* Explicitly zero out the entire IDT array to eliminate garbage addresses */
  for (int i = 0; i < IDT_MAX_DESCRIPTORS; i++) {
    idt[i].offset_low = 0;
    idt[i].selector = 0;
    idt[i].ist = 0;
    idt[i].attributes = 0; /* Mark as not present */
    idt[i].offset_middle = 0;
    idt[i].offset_high = 0;
    idt[i].reserved = 0;
  }

  
    idt_set_gate(6, (uint64_t)invalid_opcode_handler, 0x8E);

    // IRQ1 = keyboard = vector 33
    idt_set_gate(33,(uint64_t)keyboard_handler);

    load_idt();

}

void load_idt(void) {
    __asm__ volatile("lidt %0" : : "m"(idt_descriptor));
}
