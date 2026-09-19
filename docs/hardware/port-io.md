# Port I/O

## 1. Overview

Port I/O allows the CPU to communicate with hardware devices through I/O ports.


x86 CPUs have a special I/O adress spaces separate from normal memory.
1. Port 0x20 -> Master PIC command
2. Port 0x21 -> Master PIC data 
3. Port 0x60 -> Keyboard data 
4. Port 0xA0 -> Slave PIC command
5. Port 0xA1 -> Slave PIC data

In x86, the `in` and `out` instructions are used for this purpose.

---

## 2. Port I/O Functions

```c
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;

    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));

    return value;
}
```

---

## 3. `outb()`

`outb()` writes one byte to an I/O port.

```c
outb(port, value);
```

Data flow:

```text
C value → CPU → I/O port → Hardware
```

### `__asm__`

```c
__asm__
```

Tells GCC that we are inserting assembly code here.

### `volatile`

```c
__asm__ volatile
```

Tells the compiler that this assembly instruction has side effects and must
not be removed or treated like a normal calculation.

The compiler cannot simply say:

> "The C program doesn't use the result, so I'll remove this instruction."

The instruction performs an actual hardware operation.

### `"outb %0, %1"`

```c
"outb %0, %1"
```

`%0` and `%1` refer to the operands provided after the assembly string.

```text
%0 → "a"(value)
%1 → "Nd"(port)
```

So conceptually:

```asm
outb value, port
```

### `"a"(value)`

```c
"a"(value)
```

`a` means the accumulator register.

Since `value` is a `uint8_t`, the relevant register is `AL`.

```text
value → AL
```

For example:

```c
outb(0x20, 0x11);
```

The value `0x11` needs to be placed in `AL` before the `out` instruction.

Conceptually:

```asm
mov al, 0x11
```

### `"Nd"(port)`

```c
"Nd"(port)
```

This gives GCC two possibilities for the port:

```text
N → use the port number as an immediate value
d → use the DX register
```

For example:

```c
outb(0x20, 0x11);
```

GCC can use the port directly:

```asm
out 0x20, al
```

Or it can use `DX`:

```asm
mov dx, port
out dx, al
```

So basically:

```text
"Nd"(port)
→ GCC can use either an immediate port number or DX,
  whichever is appropriate.
```

---

## 4. `inb()`

`inb()` reads one byte from an I/O port.

```c
uint8_t value = inb(port);
```

Data flow:

```text
Hardware → I/O port → CPU → C variable
```

### `"=a"(value)`

```c
"=a"(value)
```

Here:

```text
= → output
a → accumulator register
```

Since we are reading one byte, the relevant register is `AL`.

So:

```text
I/O port → AL → value
```

Conceptually:

```asm
in al, port
```

The value returned in `AL` is then placed into the C variable `value`.

---

## 5. Input vs Output

The important difference between `"a"` and `"=a"`:

```text
"a"(value)
→ value is an INPUT to the assembly
→ C → CPU register

"=a"(value)
→ value is an OUTPUT from the assembly
→ CPU register → C
```

So:

```text
outb()
C → Hardware

inb()
Hardware → C
```

---

## 6. Summary

```text
outb(port, value)
    C value → AL → I/O port → Hardware

inb(port)
    Hardware → I/O port → AL → C variable
```

Important constraints:

```text
"a"  → accumulator register (AL for uint8_t)
"=a" → output through accumulator register
"Nd" → immediate port number or DX register
```

