#include<stdint.h>
#include "arch/x86_64/interrupts.h"

void print(const char *str)
{
    volatile char *video = (volatile char *)0xB8000;

    while (*str) {
        *video++ = *str++;
        *video++ = 0x07;
    }
}


void kernel_main(void){
    print("Kernel started");

    idt_init();

     while(1) {
      __asm__ volatile("hlt");
     }
}
