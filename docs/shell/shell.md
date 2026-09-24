# Shell

The shell provides an interactive command-line interface.

It is responsible for:

- Displaying the prompt
- Reading keyboard input
- Editing the current command
- Moving the command cursor
- Command history
- Parsing commands
- Executing commands
- Handling special control keys

The shell uses the keyboard driver for input and the terminal for output.

---

## Shell Architecture

The input path is:

```text
Keyboard hardware
       |
       v
Keyboard driver
       |
       v
Keyboard key buffer
       |
       v
Shell
       |
       +--> Parser
       |
       +--> Commands
       |
       v
Terminal
```

The keyboard driver converts hardware scancodes into logical keys.

The shell interprets those keys.

---

## Shell State

The shell maintains:

```c
static char input_buffer[SHELL_BUFFER_SIZE];

static unsigned int input_length;
static unsigned int cursor_position;
static unsigned int shell_start_row;
```

### Input Buffer

Stores the command currently being edited.

Example:

```text
> echo hello
  ^^^^^^^^^^
  input_buffer
```

### Input Length

Stores the number of characters currently in the input.

### Cursor Position

Stores the cursor position relative to the command input.

It is not the absolute terminal column.

For example:

```text
> hello
  ^
  0
```

If the cursor is after `he`:

```text
> hello
    ^
    2
```

### Shell Start Row

Stores the terminal row where the current prompt begins.

This is important when the command spans multiple terminal rows.

---

## Shell Configuration

Shell configuration is centralized in:

```text
shell_config.h
```

Current values include:

```c
#define SHELL_BUFFER_SIZE 256
#define SHELL_HISTORY_SIZE 16
#define SHELL_PROMPT "> "
#define SHELL_PROMPT_COLUMN 2
```

This avoids hardcoding the prompt throughout the shell implementation.

---

## Shell Initialization

The shell initialization function resets shell state:

```text
input_length     = 0
cursor_position  = 0
shell_start_row  = current terminal row
history          = initialized
```

Initialization does not print the prompt.

The prompt is printed separately by:

```c
shell_print_prompt();
```

This keeps initialization focused on state rather than UI output.

---

## Prompt

The shell prompt is:

```text
> 
```

The prompt column is:

```text
2
```

The shell treats the prompt as outside the editable input.

For:

```text
> hello
```

the input begins at column:

```text
2
```

and `h` has input position:

```text
0
```

---

## Input Cursor

The shell has its own input cursor:

```c
cursor_position
```

The absolute terminal position is calculated from:

```text
SHELL_PROMPT_COLUMN + cursor_position
```

For example:

```text
Prompt:

> hello
  ^

SHELL_PROMPT_COLUMN = 2
cursor_position = 0
```

The shell then converts the absolute position into:

```text
row offset
column
```

using:

```text
row_offset = absolute_position / TERMINAL_WIDTH
column     = absolute_position % TERMINAL_WIDTH
```

The target terminal row is:

```text
shell_start_row + row_offset
```

---

## Cursor Movement

The shell supports:

- Left arrow
- Right arrow
- Home
- End

### Left

Moves the input cursor one character toward the beginning.

```text
> hello
     ^
```

becomes:

```text
> hello
    ^
```

The cursor cannot move before position `0`.

### Right

Moves the cursor one character toward the end.

The cursor cannot move beyond:

```text
input_length
```

### Home

Moves the cursor to:

```text
cursor_position = 0
```

### End

Moves the cursor to:

```text
cursor_position = input_length
```

---

## Character Insertion

Characters are inserted at the current cursor position.

Example:

```text
> helo
     ^
```

Typing `l` inserts it before the current character.

The existing characters are shifted right.

Conceptually:

```text
Before:

h e l o
    ^

After inserting:

h e l l o
      ^
```

The shell performs this by shifting the buffer from right to left.

After insertion:

```text
input_length++;
cursor_position++;
```

---

## Backspace

Backspace removes the character before the cursor.

Example:

```text
> hello
      ^
```

Backspace removes `o`.

The remaining characters are shifted left.

Then:

```text
input_length--;
cursor_position--;
```

The cursor cannot move before the beginning of the input.

---

## Delete

Delete removes the character at the current cursor position.

Example:

```text
> hello
    ^
```

Delete removes the character under the cursor.

Characters after it are shifted left.

Unlike Backspace, Delete does not move the cursor backward.

---

## Redrawing Input

The shell cannot simply print a newly typed character because characters
may be inserted in the middle of the command.

For example:

```text
> helo
     ^
```

After inserting `l`:

```text
> hello
      ^
```

Therefore the shell redraws the editable region.

The redraw process is:

```text
1. Determine the old and new lengths.
2. Clear the affected screen positions.
3. Write the current input buffer.
4. Restore the logical cursor position.
```

The shell uses:

```c
terminal_put_at();
```

for exact screen updates.

---

## Multi-Line Input

The terminal width is limited.

A long command can continue onto another row:

```text
> this-is-a-very-long-command-that-reaches-the
next-screen-row
```

The shell converts the logical input position into a terminal row and
column.

```text
absolute_position =
    SHELL_PROMPT_COLUMN + cursor_position

row_offset =
    absolute_position / TERMINAL_WIDTH

column =
    absolute_position % TERMINAL_WIDTH
```

This allows the shell to treat the input as one continuous logical string
even though it occupies multiple screen rows.

---

## Scrolling During Redraw

If input reaches the bottom of the screen, the shell may need to scroll.

The shell checks whether the required row is outside the terminal.

When scrolling occurs:

```text
terminal_scroll();
shell_start_row--;
```

The shell adjusts `shell_start_row` because the existing command has moved
up by one screen row.

---

## Enter

When Enter is received:

```text
1. Terminate the input string.
2. Add it to history.
3. Move the cursor to the end of the input.
4. Move to the next terminal line.
5. Parse the command.
6. Execute the command.
7. Reset the input state.
8. Print a new prompt.
9. Record the new prompt row.
```

The input is terminated with:

```c
input_buffer[input_length] = '\0';
```

This converts the buffer into a C string.

---

## Command Processing

The shell passes the input buffer to the parser.

The parser produces:

```text
argc
argv
```

For:

```text
echo hello world
```

the result is conceptually:

```text
argc = 3

argv[0] = "echo"
argv[1] = "hello"
argv[2] = "world"
```

The shell then passes these arguments to the command system.

---

## Parser

The parser supports:

- Spaces
- Tabs
- Single quotes
- Double quotes
- Escapes inside quotes

Whitespace separates arguments when it is outside quotes.

Example:

```text
echo hello world
```

produces:

```text
echo
hello
world
```

Quoted text remains one argument:

```text
echo "hello world"
```

produces:

```text
echo
hello world
```

---

## Quotes

The parser tracks the current quote:

```text
quote = 0
```

No quote is active initially.

When it encounters:

```text
'
```

or:

```text
"
```

it enters quote mode.

The matching quote exits quote mode.

For example:

```text
echo "hello world"
```

The space inside the quotes is not treated as an argument separator.

---

## Escaping

Inside quotes, a backslash escapes the following character.

Conceptually:

```text
\"
```

means that the `"` is treated as a literal character rather than ending
the quote.

The parser also detects an incomplete escape at the end of a quoted
argument.

---

## Parser Errors

The shell handles parser errors such as:

```text
Too many arguments
```

and:

```text
Unclosed quote
```

The parser reports these using a result value.

The shell decides how to display the error.

This keeps parsing logic separate from shell UI.

---

## Command History

The shell maintains a fixed-size history.

Current configuration:

```c
#define SHELL_HISTORY_SIZE 16
```

History stores previously entered commands.

The history implementation uses:

```text
history_start
history_count
history_position
```

This allows the history to behave like a circular buffer.

---

## History Navigation

The shell supports:

- Arrow Up
- Arrow Down

### Arrow Up

Requests the previous command.

If one exists, the shell loads it into the input buffer.

### Arrow Down

Requests the next command.

If there is no newer command, the shell clears the current input.

---

## Loading History

When a history command is loaded:

```text
1. Copy command into input_buffer.
2. Calculate input_length.
3. Set cursor_position to the end.
4. Redraw the input.
```

The shell therefore treats a history command like editable input rather
than immediately executing it.

---

## History Reset

When the user edits the command manually, history navigation is reset.

This prevents the shell from remaining in an old history position after
the user starts editing.

---

## Ctrl+C

Ctrl+C is recognized by the keyboard driver as:

```c
KEY_CTRL_C
```

The shell interprets it as:

```text
Cancel current input
```

It does not terminate the shell.

The shell:

```text
1. Clears the current input.
2. Prints a newline.
3. Prints a new prompt.
4. Records the new prompt row.
```

This behavior can later be extended when the OS has real process
management.

---

## Ctrl+L

Ctrl+L is intended to clear the terminal.

The important design is that both:

```text
Ctrl+L
```

and:

```text
clear
```

should use the same terminal clearing operation.

The shell should not duplicate the low-level screen-clearing logic.

The relationship is:

```text
Ctrl+L
   |
   v
terminal_clear()

clear command
   |
   v
terminal_clear()
```

---

## Special Keys

Normal character input is handled first:

```c
if (key < 0x100)
```

Special keys use values outside the character range.

Therefore shell key handling is organized conceptually as:

```text
Normal characters
        |
        v
Special keys
        |
        +--> Backspace
        +--> Delete
        +--> Enter
        +--> Arrow keys
        +--> Home / End
        +--> History
        +--> Ctrl+C
        +--> Ctrl+L
```

This keeps normal text input separate from control operations.

---

## Shell and Keyboard Responsibilities

The keyboard driver handles hardware-specific details:

```text
Scancode
   |
   v
Modifier state
   |
   v
Logical key
```

The shell handles meaning:

```text
KEY_ARROW_LEFT -> move shell cursor
KEY_BACKSPACE  -> edit input
KEY_CTRL_C     -> cancel input
KEY_ENTER      -> execute command
```

The keyboard driver should not know what the shell does with those keys.

---

## Shell and Terminal Responsibilities

The shell owns:

```text
input buffer
input cursor
history
parser
commands
prompt
```

The terminal owns:

```text
VGA memory
screen position
hardware cursor
scrolling
character output
```

This separation is important.

The shell tells the terminal what to display, but the terminal does not
know why it is displaying it.

---

## Current Shell Flow

The complete interactive flow is:

```text
Keyboard interrupt
       |
       v
keyboard_handler()
       |
       v
keyboard buffer
       |
       v
keyboard_getkey()
       |
       v
shell_handle_key()
       |
       +-------------------+
       |                   |
   normal key          special key
       |                   |
       v                   v
edit input             shell action
       |                   |
       +---------+---------+
                 |
                 v
        shell_redraw_input()
                 |
                 v
              Terminal
```

When Enter is pressed:

```text
Enter
  |
  v
Terminate input
  |
  v
Parse
  |
  v
argc / argv
  |
  v
Execute command
  |
  v
Reset input
  |
  v
Print prompt
```

---

## Current Design Boundary

The shell currently provides an interactive command-line interface.

It does not yet provide:

- Processes
- Threads
- Program execution
- Process creation
- Process termination
- Scheduling
- Filesystem-backed commands
- Pipes
- Job control

Those features belong to later parts of the operating system.
