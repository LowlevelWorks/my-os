## Master PIC 
0x20 -> command 
0x21 -> data 

## Slave PIC
0xA0 -> command
0xA1 -> data

-----------------------------
outb(PIC1_DATA, 32);
outb(PIC2_DATA, 40);

Master PIC:

IRQ0 -> vector 32
IRQ1 -> vector 33 -> keyboard 
IRQ2 -> vector 34 
....
IRQ7 -> vector 39


Slave PIC:

IRQ8 -> vector 40
IRQ9 -> vector 41 
....
IRQ15 -> vector 47

-------------------------------------

Keyboard -> IRQ1 -> Master PIC -> Vector 33 -> IDT[33] -> keyboard_handler

0x11 -> tells the PIC, I'm going to initialize you. Pay attention to the next configuration commands.

outb(PIC1_DATA, 32);  // Master starts at vector 32
outb(PIC2_DATA, 40);  // Slave starts at vector 40

why slave start at 40?
-> The master gets 8 vectors 

32 -> IRQ0
33 -> IRQ1
...
39 -> IRQ7

Then the slave gets the next 8:
40 -> IRQ8
41 -> IRQ9
42 -> IRQ10
...
47 -> IRQ15

             MASTER PIC
             ┌─────────┐
IRQ0 ───────►│         │──► Vector 32
IRQ1 ───────►│         │──► Vector 33  ← KEYBOARD
IRQ2 ───────►│         │──► Vector 34
IRQ3 ───────►│         │──► Vector 35
IRQ4 ───────►│         │──► Vector 36
IRQ5 ───────►│         │──► Vector 37
IRQ6 ───────►│         │──► Vector 38
IRQ7 ───────►│         │──► Vector 39
             └────┬────┘
                  │
                IRQ2
                  │
             ┌────▼────┐
             │  SLAVE  │
IRQ8 ───────►│         │──► Vector 40
IRQ9 ───────►│         │──► Vector 41
...          │         │
IRQ15 ──────►│         │──► Vector 47
             └─────────┘




 x86 CPUs have a special I/O adress spaces separate from normal memory.
- Port 0x20 -> Master PIC command
- Port 0x21 -> Master PIC data 
- Port 0x60 -> Keyboard data 
- Port 0xA0 -> Slave PIC command
- Port 0xA1 -> Slave PIC data

e.g out 0x20, something [outb(port,value)] 
-> Send a value to hardware port 0x20.

and
 - in something, 0x60  [inb(port)]
 Read a value from hardware port 0x60

