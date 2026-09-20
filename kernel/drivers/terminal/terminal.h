#ifndef TERMINAL_H
#define TERMINAL_H

#define TERMINAL_WIDTH 80
#define TERMINAL_HEIGHT 25

void terminal_init(void);
void terminal_putchar(char c);
void terminal_backspace(void);
void terminal_write(const char *str);
void terminal_clear(void);
void terminal_set_cursor(unsigned int column);
unsigned int terminal_get_row(void);
void terminal_move_cursor_row(int offset);
void terminal_scroll(void);
void terminal_put_at(unsigned int row, unsigned int column, char c);
void terminal_write_at(unsigned int column, const char *str);

#endif
