#include<stdint.h>
#include "arch/x86_64/interrupts.h"
#include "arch/x86_64/pic.h"

void print(const char *str)
{
    volatile char *video = (volatile char *)0xB8000;

    while (*str) {
        *video++ = *str++;
        *video++ = 0x07;
    }
}


void kernel_main(void){

    idt_init();
    pic_remap();


    print("Kernel started");


     while(1) {
      __asm__ volatile("hlt");
     }
}
