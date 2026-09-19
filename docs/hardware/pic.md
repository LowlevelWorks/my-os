# 8259 PIC(Programmable Interrupt Controller)

The PIC is responsible for recieving hardware interrupt request(IRQ)
and sending the corresponding interrupt to the CPU.


## 1. Our Hardware Relationship

```text
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

Master -> IRQ0 - IRQ7
Slave  -> IRQ8 - IRQ15
Slave PIC is connected to IRQ2 of the master PIC.
```

## 2. Configuration

ICW = Initalization Command Word
The 8259 expects configuration in this order:

1. ICW1 -> "Start Initialization" (command)
2. ICW2 -> "My interrupt vectors start here" (data)
3. ICW3 -> "How am I connected to other PICs?" (data)
4. ICW4 -> "Which operating mode?" (data)

### PIC Ports
Each PIC has two important IO ports:


| PIC    | Command |  Data |
|--------|-------- |-------|
| Master | 0x20    | 0x21  |
| Slave  | 0xA0    |  0xA1 |


The Command PORT is used to tell the PIC what operation to perform.
e.g: PIC, start your initialization sequence,
     PIC, the interrupt has been handled. This is the EOI Command.

The Data PORT is used for configuration/data values.
e.g Set the master's interrupt-vector offset to 32.
    Set the master's interrupt mask

### ICW1

ICW1 starts the initialization process

```c
  outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
```


| Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
|-------|-------|-------|-------|-------|-------|-------|-------|
| 0     | 0     | 0     | 1     | 0     | 0     | SNGL     | ICW4     |

Bit0 indiccates whether the ICW4 is required or not.If Bit0 = 0, ICW4 is not 
required(ignore if even given later) but if BIT0 = 1, ICW4 is required

Bit1 is used to indicate whether the 8259 PIC is cascaded or not. 
if Bit1=1, Single 8259 is used and B1 = 0, Cascaded 8259 is used.

Bit4 always should be 1 for initialization, then only PIC enter under intialization

so Here since we are giving ICW4 for operating mode (to chose 8086 mode): Bit0 = 1 -> 0x01
Also since out hardware architecture is cascaded already, we need Bit1 = 0
and Bit4 = 1. so we need command 00010001(0x11).

Here 0x10  =  0001 0000 (ICW1_INIT)
and 0x01 =    0000 0001 (ICW1_ICW4)
OR       =    0001 0001 (0x11)

so Final command for ICW1 = 0x11 (ICW1_INIT | ICW1_ICW4)
Initialize both **Master(PIC1_COMMAND)** and **Slave(PIC2_COMMAND)**.

### ICW2

ICW2 - Interrupt vector offset. It answer one question:
When this PIC recieves IRQ number N, which cpu vector should it generate?.
ICW2 tells the PIC its vector offsets.

#### IRQ vs Vector

An IRQ is the number used by the PIC.
An interrupt vector is the number used by the CPU to index the IDT.

Hardware --IRQ--> PIC --Vector--> CPU -> `IDT[vector]`


| Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
|-------|-------|-------|-------|-------|-------|-------|-------|
| N7     | N6     | N5     | N4     | N3    | N2     | N1     | N0  |


Without ICW2, the PIC would use its default interrupt vectors:

```text
Master PIC : IRQ0–IRQ7 → INT 0x08–0x0F
Slave PIC  : IRQ8–IRQ15 → INT 0x70–0x77
```

This causes a problem because CPU exceptions already use:

```text
INT 0x00–0x1F
```

For example:

```text
INT 0x08 → Double Fault exception
IRQ0      → Timer interrupt
```

Both would use interrupt vector `0x08`, creating a conflict.

Therefore, we remap the PIC using ICW2.

ICW2 specifies the **base interrupt vector** for a PIC.

```text
Actual Interrupt Vector = ICW2 + IRQ Number
```


```c
  outb(PIC1_DATA, 32);
  outb(PIC2_DATA, 40);
```

IRQ1 → 32 + 1 → Vector 33 → IDT[33]


## ICW3

ICW3 is the one that tells the two PICs how they are physically connected to each other.

```c
outb(PIC1_DATA, 0x04);
outb(PIC2_DATA, 0x02);
```
Master IR2 -> Slave. so Master needs to know "My IR2 pin has a slave connected to it."
Slave needs to know: "I am connected to the master's IR2".

1. 0x04 = 0000 0100

Bit:    7   6   5   4   3   2   1   0
        │   │   │   │   │   │   │   │
Value:  0   0   0   0   0   1   0   0
                            ↑
                           IR2

For Master PIC, each bit represents one of its IR lines. A bit of 1 means There is slave connected 
to this IR line. A bit of 0 means there is no Slave PIC connected here.

2. 0x02 = 0000 0010
The slave interprets ICW3 differently. 
For the slave, ICW3 tells which IR Lines of the master am I connected to ?. The Value represents the Master's IR number

## ICW4



| Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
|-------|-------|-------|-------|-------|-------|-------|-------|
| 0     | 0     | 0     | SFNM    | BUF   | M/S     | AEOI    | µPM |


- When AEOI = 1, then Automatic end of interrupt mode is selected.
- When SFMN = 1, then a special fully nested mode is selected.
- µPM (microprocessor mode) = 1, then 8086 operations are performed, othersisw 8085 operations are performed.

For now, we are only interested in first bit(Bit 0) -> set the bit 1, so we can change into 8086 mode.
i.e 00000001 = 0x01

```c
  #define ICW4_8086 0x01

  outb(PIC1_DATA, ICW4_8086);
  outb(PIC2_DATA, ICW4_8086);

```

## 2. PIC UNMASK

The PIC has an interrupt mask register(IMR).
Each PIC has 8 bits:

bit:    7 6 5 4 3 2 1 0
       ─────────────────
IRQ:    7 6 5 4 3 2 1 0

example: 1111111 means blocked all IRQ0 - IRQ7 -> 0xFF

```text
1 = masked = blocked
0 = unmasked = allowed
```

```c
void pic_unmask_irq(unsigned char irq) {
  unsigned short port;

  if (irq < 8)
    port = PIC1_DATA;
  else
    port = PIC2_DATA;

  unsigned char mask = inb(port);

  mask &= ~(1 << (irq % 8));

  outb(port, mask);
}
```

Enables a specific hardware IRQ by clearing its mask bit in the PIC's Interrupt Mask Register (IMR).

```text
mask &= ~(1 << (irq % 8));

For keyboard IRQ1:
irq %8 = 1 %8 = 1
1 << 1 = 00000010
~00000010 = 11111101 

current mask: 11111111
               &
              11111101
              ---------
              11111101

Bit 1 becomes 0, so IRQ1 is unmasked.
```

## EOI — End Of Interrupt

**EOI means End Of Interrupt.**

Its job is to tell the PIC:

> "I have finished handling the interrupt you gave me."

---

### 1. Why do we need EOI?

Suppose the keyboard generates:

```text
Keyboard
   ↓
IRQ1
   ↓
PIC
   ↓
CPU
   ↓
keyboard_handler()
```

The PIC has delivered the interrupt to the CPU.

While handling it, the PIC considers that interrupt **in service**.

When your handler finishes, you need to tell the PIC:

```text
"I am done with this interrupt."
```

That's EOI.

---

## 2. The EOI command

For the 8259 PIC:

```c
#define PIC_EOI 0x20
```

We send it to the **command port**:

```c
outb(PIC1_COMMAND, PIC_EOI);
```

Since:

```c
PIC1_COMMAND = 0x20
```

this becomes:

```c
outb(0x20, 0x20);
```

```text
0x20 → I/O port
0x20 → EOI command
```

---

## 3. What happens without EOI?

Imagine:

```text
Keyboard press
      ↓
IRQ1
      ↓
PIC
      ↓
CPU
      ↓
keyboard_handler
```

The PIC marks IRQ1 as **in service**.

If your handler returns with:

```asm
iretq
```

but never sends EOI, the PIC isn't properly informed that the interrupt has been handled.

That's why the normal handler flow is:

```text
Interrupt arrives
      ↓
CPU enters handler
      ↓
Read keyboard data
      ↓
Send EOI
      ↓
iretq
```

---

## 4. EOI is NOT the same as unmasking

This distinction is important.

### Unmask

```c
pic_unmask_irq(1);
```

means:

> "Allow IRQ1 to generate interrupts."

Usually done during initialization.

### EOI

```c
pic_send_eoi(1);
```

means:

> "I finished handling this particular interrupt."

Done **every time that interrupt occurs**.

So:

```text
BOOT
 ↓
unmask IRQ1          ← once
 ↓
keyboard interrupt
 ↓
handler
 ↓
EOI                  ← every interrupt
 ↓
iretq
 ↓
keyboard interrupt
 ↓
handler
 ↓
EOI                  ← every interrupt
 ↓
...
```

---

## 5. Why does `irq >= 8` matter?

Remember our two PICs:

```text
Master PIC
IRQ0 - IRQ7

Slave PIC
IRQ8 - IRQ15
```

The Slave is connected to the Master's IRQ2:

```text
              Master
                │
               IR2
                │
                ▼
              Slave
```

Therefore, an interrupt from the Slave passes through **both PICs**.

For example:

```text
IRQ12
  ↓
Slave PIC
  ↓
Master PIC
  ↓
CPU
```

So we need:

```c
outb(PIC2_COMMAND, PIC_EOI);
outb(PIC1_COMMAND, PIC_EOI);
```

---

## 6. That's why our function does this

```c
void pic_send_eoi(unsigned char irq)
{
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }

    outb(PIC1_COMMAND, PIC_EOI);
}
```

For IRQ1:

```text
IRQ1
 ↓
Master
 ↓
EOI Master
```

For IRQ12:

```text
IRQ12
 ↓
Slave
 ↓
EOI Slave
 ↓
Master
 ↓
EOI Master
```

---

## 7. Why Slave first?

Because the interrupt came from:

```text
Slave → Master → CPU
```

So we acknowledge the source first:

```text
Slave EOI
   ↓
Master EOI
```

This tells the chain:

```text
Slave:  "I'm done."
Master: "I'm done too."
```

---
## Legacy PIC vs Modern APIC

Older x86 PCs used the **8259 PIC** to handle hardware interrupts.

```text
Hardware
   ↓
IRQ
   ↓
8259 PIC
   ↓
Vector
   ↓
IDT
   ↓
CPU Handler
```

Classic setup:

```text
Master PIC → IRQ0–IRQ7
Slave PIC  → IRQ8–IRQ15
```

---

## Modern — APIC

Modern x86 systems normally use **APIC** instead.

```text
Hardware
   ↓
I/O APIC
   ↓
Local APIC
   ↓
CPU
   ↓
IDT
   ↓
Handler
```

APIC supports modern features such as **multiple CPUs and more flexible interrupt routing**.

---

## Why are we still handling the 8259?

Because it is part of the **legacy x86 architecture** and is still relevant for compatibility and system startup.

Our OS may encounter a legacy PIC before switching to APIC.

If we use APIC, we can disable legacy PIC interrupts:

```c
void pic_disable(void)
{
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
```

```text
0xFF = 11111111

1 = blocked
0 = allowed

IRQ0–IRQ15 → blocked
```

**We learn PIC first because it is simpler and provides the foundation for understanding hardware interrupts.**
