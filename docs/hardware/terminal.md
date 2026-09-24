# Terminal

The terminal provides text output to the VGA text-mode screen.

It is responsible for:

- Writing characters
- Writing strings
- Tracking cursor position
- Moving the cursor
- Handling new lines
- Scrolling
- Updating the hardware cursor
- Writing at an exact screen position

The terminal does not know anything about the shell.

---

## VGA Text Mode

The VGA text buffer starts at:

```c
0xB8000
```

The screen is:

- 80 columns
- 25 rows

Each screen cell uses 16 bits:

```text
15                    8 7                     0
+----------------------+----------------------+
|        Color         |      Character       |
+----------------------+----------------------+
```

The lower 8 bits contain the character.

The upper 8 bits contain the color.

For example:

```c
VGA_MEMORY[position] =
    ((uint16_t)color << 8) | (uint8_t)c;
```

---

## Screen Position

The terminal tracks:

```c
static unsigned int row = 0;
static unsigned int column = 0;
```

A screen position can be converted into a linear VGA-buffer index:

```text
position = row * VGA_WIDTH + column
```

For an 80-column screen:

```text
row = 0, column = 0  -> 0
row = 0, column = 1  -> 1
row = 1, column = 0  -> 80
row = 2, column = 0  -> 160
```

This allows the two-dimensional screen to be accessed as a one-dimensional
array.

---

## Writing a Character

The basic terminal operation is writing one character at the current
cursor position.

Conceptually:

```text
current row
     +
     |
     v
+---+---+---+---+
|   |   | X |   |
+---+---+---+---+
        ^
      column
```

After writing a normal character:

```text
column++
```

When the column reaches the end of the screen, the terminal moves to the
next row.

---

## New Line

A newline does not write a visible character to VGA memory.

Instead, it changes the cursor position.

Conceptually:

```text
column = 0
row++
```

If the cursor is already on the last row, the terminal must scroll.

---

## Scrolling

The terminal uses scrolling when the cursor reaches the bottom of the
screen.

The screen contents are moved upward:

```text
Before:

row 0
row 1
row 2
...
row 23
row 24


After scroll:

row 0 <- old row 1
row 1 <- old row 2
row 2 <- old row 3
...
row 23 <- old row 24
row 24 <- blank
```

The terminal exposes:

```c
terminal_scroll();
```

This is also useful to higher-level components such as the shell when
redrawing input that reaches the bottom of the screen.

---

## Exact-Position Output

The terminal provides:

```c
void terminal_put_at(
    unsigned int row,
    unsigned int column,
    char c
);
```

This writes a character at an exact screen position without changing the
logical cursor.

The position is calculated as:

```c
row * VGA_WIDTH + column
```

Bounds are checked before accessing VGA memory.

This is useful for shell input redrawing because the shell may need to
rewrite characters at positions other than the current cursor.

---

## Logical Cursor vs Hardware Cursor

There are two related concepts.

### Logical Cursor

The terminal keeps:

```c
row
column
```

These describe where the terminal believes the cursor is.

### Hardware Cursor

The VGA controller also has a visible text cursor.

The terminal updates it through VGA I/O ports:

```c
#define VGA_INDEX_PORT 0x3D4
#define VGA_DATA_PORT  0x3D5
```

The cursor position is:

```c
uint16_t position = row * VGA_WIDTH + column;
```

The VGA controller stores this position as two bytes.

Low byte:

```c
position & 0xFF
```

High byte:

```c
(position >> 8) & 0xFF
```

The hardware cursor is updated by writing these bytes to VGA registers.

---

## Cursor Registers

The low cursor byte uses register:

```text
0x0F
```

The high cursor byte uses register:

```text
0x0E
```

The sequence is:

```c
outb(VGA_INDEX_PORT, 0x0F);
outb(VGA_DATA_PORT, position & 0xFF);

outb(VGA_INDEX_PORT, 0x0E);
outb(VGA_DATA_PORT, (position >> 8) & 0xFF);
```

The index port selects the VGA register.

The data port writes the value into that register.

---

## Setting the Cursor Column

The terminal provides:

```c
terminal_set_cursor(unsigned int new_column);
```

The requested column is checked against the terminal width.

This prevents the cursor from being placed outside the valid screen area.

---

## Moving the Cursor by Rows

The terminal also provides:

```c
terminal_move_cursor_row(int offset);
```

This is useful when another component needs to move the cursor vertically.

For example:

```text
current row: 10

offset: -1

new row: 9
```

The shell uses this when its input spans multiple terminal rows.

---

## Getting the Current Row

The terminal provides:

```c
terminal_get_row();
```

This allows higher-level components to know where the terminal currently
is.

The shell uses this when establishing the beginning of a new command line.

---

## Terminal Initialization

The terminal initializes its internal state:

```text
row    = 0
column = 0
color  = default color
```

It then updates the hardware cursor.

The terminal initialization is performed before the shell starts.

---

## Terminal and Shell Separation

The terminal is a low-level output component.

The shell is a higher-level input component.

The relationship is:

```text
Keyboard
   |
   v
 Shell
   |
   v
Terminal
   |
   v
VGA
```

The terminal does not know:

- What a command is
- What a prompt is
- What history means
- What a shell argument is

The shell uses terminal operations to display its interface.

---

## Important Design Principle

The terminal owns terminal state.

The shell owns shell state.

For example:

```text
Terminal
├── row
├── column
├── color
└── VGA cursor

Shell
├── input buffer
├── input length
├── cursor position
├── history
└── command parsing
```

Keeping these responsibilities separate makes both components easier to
extend.
