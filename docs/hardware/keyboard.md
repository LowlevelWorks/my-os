# Keyboard Driver

A keyboard driver sits between the **physical keyboard** and the rest of the operating system.

The keyboard does not directly send characters like `a`, `A`, or `Enter`. It sends **scancodes**. The driver interprets those scancodes and converts them into values that the kernel can understand.

```text
Keyboard
   ↓
Scancode
   ↓
Keyboard Driver
   ↓
Character / Special Key
   ↓
Keyboard Buffer
   ↓
Kernel / Terminal
```

## 1. Keyboard Data Port

A PS/2 keyboard sends its scancodes through I/O port `0x60`.

```c
#define KEYBOARD_DATA_PORT 0x60
```

The driver reads the scancode using:

```c
uint8_t scancode = inb(KEYBOARD_DATA_PORT);
```

### Why?

The keyboard hardware communicates with the CPU through an I/O port. Port `0x60` is where the keyboard controller provides keyboard data.

---

## 2. Scancodes Are Not Characters

A keyboard sends a numeric **scancode** for each key.

For example, in PS/2 Set 1:

```text
A key     → 0x1E
B key     → 0x30
Enter     → 0x1C
Shift     → 0x2A
```

The driver translates these numbers into meaningful values:

```text
0x1E → 'a'
0x30 → 'b'
0x1C → '\n'
```

This translation is done using a lookup table:

```c
static const char keyboard_map[128] = {
    ...
};
```

### Why 128 entries?

Normal Set 1 scancodes use the lower 7 bits (`0x00–0x7F`).

The high bit (`0x80`) is used to indicate a key release.

```text
0x1E → A pressed
0x9E → A released
       ↑
   release bit
```

Therefore, the normal keyboard map only needs entries for the base scancodes.

Keys such as Shift, Ctrl, Caps Lock, and function keys are handled separately because they do not produce normal characters.

---

## 3. Key Press and Key Release

The keyboard sends a scancode when a key is pressed and another scancode when that key is released.

In PS/2 **Set 1**, these are called:

- **Make code** — key pressed
- **Break code** — key released

For most normal keys, the break code is the make code with bit 7 (`0x80`) set.

For example, the `A` key:

```text
A pressed   → 0x1E
A released  → 0x9E
```

The difference is:

```text
0x1E = 0001 1110
0x9E = 1001 1110
       ↑
    release bit
```

So the high bit tells the driver whether the key is being pressed or released.

### Detecting the release

The driver checks bit 7:

```c
if (scancode & 0x80) {
    // Key released
}
```

To get the original key code without the release bit:

```c
uint8_t code = scancode & 0x7F;
```

For example:

```text
Scancode       0x9E
       ↓
0x9E & 0x7F
       ↓
     0x1E
       ↓
      'A' key
```

### Why does the driver need key release information?

Some keys have a state while they are being held.

For example, when Shift is pressed:

```text
Shift pressed
      ↓
shift_pressed = 1
```

While Shift is held, pressing `A` produces uppercase `A`.

When Shift is released:

```text
Shift released
      ↓
shift_pressed = 0
```

The driver can then return to normal character processing.

This is why the keyboard driver must distinguish between:

```text
Key pressed  → update state / process key
Key released → update state
```

Normal character keys usually only need to be processed when they are pressed. 
Modifier keys such as **Shift, Ctrl, and Alt** need both press and release events so their state can be maintained correctly.
---

## 4. Modifier Keys

Some keys do not represent characters themselves.

For example:

```text
Shift
Ctrl
Alt
Caps Lock
```

Instead, they change how other keys should be interpreted.

The driver therefore maintains their state:

```c
static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int alt_pressed = 0;
static int caps_lock = 0;
```

For example:

```text
Shift pressed
     ↓
shift_pressed = 1
     ↓
A pressed
     ↓
'A'
```

Caps Lock is different because it is a toggle:

```text
Caps Lock pressed
       ↓
caps_lock = 1

Caps Lock pressed again
       ↓
caps_lock = 0
```

---
## 5. Extended and Special Keys

Not every keyboard key produces a normal character.

For example:

```text
A       → 'a'
B       → 'b'
1       → '1'
```

But keys such as:

```text
Arrow Up
Arrow Down
Home
End
Delete
Insert
F1–F12
```

do not represent normal ASCII characters. The OS therefore represents them using its own **special key codes**.

```c
#define KEY_ARROW_UP    0x100
#define KEY_ARROW_DOWN  0x101
#define KEY_ARROW_LEFT  0x102
#define KEY_ARROW_RIGHT 0x103

#define KEY_HOME        0x104
#define KEY_END         0x105
#define KEY_PAGE_UP     0x106
#define KEY_PAGE_DOWN   0x107
```

### Extended Scancodes

Some keys are sent by the keyboard using more than one byte.

These sequences usually begin with the `0xE0` prefix.

For example:

```text
E0 48 → Arrow Up
E0 50 → Arrow Down
E0 4B → Arrow Left
E0 4D → Arrow Right
```

The `0xE0` byte tells the driver:

> The next scancode belongs to an extended sequence.

The driver remembers this using:

```c
static int extended_prefix = 0;
```

When `0xE0` is received:

```c
if (scancode == 0xE0) {
    extended_prefix = 1;
    return;
}
```

The next scancode is then passed to the extended-key handler.

### Extended vs Special

These terms describe two different things:

```text
Extended
    ↓
How the keyboard sends the scancode

Special
    ↓
How the OS represents the key
```

For example:

```text
Physical key: Arrow Up
       ↓
Keyboard sends: E0 48
       ↓
Extended scancode
       ↓
Driver translates it
       ↓
KEY_ARROW_UP
       ↓
Special key
```

Not every special key is extended.

For example, F1 has a normal one-byte scancode:

```text
F1
 ↓
0x3B
 ↓
KEY_F1
```

So:

> **Extended describes the keyboard's scancode sequence, while special describes the key's representation inside the OS.**---

## 7. Keyboard Buffer

A keyboard interrupt can happen at any time.

The kernel may not be ready to process the key immediately.

So the driver stores processed keys in a **ring buffer**.

```c
static uint16_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];

static volatile unsigned int buffer_head = 0;
static volatile unsigned int buffer_tail = 0;
```

Conceptually:

```text
Keyboard
   ↓
Interrupt
   ↓
Keyboard Driver
   ↓
Ring Buffer
   ↓
Kernel reads later
```

For example, if the user types:

```text
hello
```

the buffer may contain:

```text
'h' 'e' 'l' 'l' 'o'
 ↑
tail
```

The kernel consumes the keys one at a time.

---

## 8. Why Use a Ring Buffer?

A ring buffer allows the keyboard driver and kernel to work independently.

The keyboard can continue receiving input while the kernel is doing other work.

```text
             Keyboard
                 │
                 ▼
        ┌─────────────────┐
        │   Ring Buffer   │
        └─────────────────┘
                 │
                 ▼
               Kernel
```

`buffer_head` points to where the next key will be written.

`buffer_tail` points to the next key that should be read.

When the end of the array is reached, the indexes wrap around to the beginning.

---

## 9. Driver Interface

The rest of the kernel should not need to understand PS/2 scancodes.

It only needs a simple interface:

```c
int keyboard_has_input(void);
uint16_t keyboard_getkey(void);
```

For example:

```c
if (keyboard_has_input()) {
    uint16_t key = keyboard_getkey();
}
```

This gives us a clean separation:

```text
                Keyboard Driver
                       │
        ┌──────────────┴──────────────┐
        │                             │
 Hardware details              OS-level keys
 PS/2 / scancodes              'a', Enter, F1
 0x60 / E0 prefix              KEY_ARROW_UP
        │                             │
        └──────── hidden ─────────────┘
```

The **terminal, shell, or other kernel component should consume the key** rather than the keyboard driver deciding what to do with it.

---

## 10. Overall Concept

The keyboard driver performs five main jobs:

```text
1. Read
   Keyboard → scancode

2. Recognize
   Normal / release / modifier / extended

3. Translate
   Scancode → character or special key

4. Buffer
   Store the result in the ring buffer

5. Provide
   Kernel → keyboard_getkey()
```

The key idea is:

> **The keyboard driver converts hardware-level keyboard events into OS-level key events.**

This separation allows the **terminal, shell, and command-line editor** to be built on top of the keyboard driver without needing to know anything about PS/2 scancodes.
