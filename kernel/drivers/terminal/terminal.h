#ifndef TERMINAL_H
#define TERMINAL_H

void terminal_init(void);
void terminal_putchar(char c);
void terminal_backspace(void);
void terminal_write(const char *str);
void terminal_clear(void);
void terminal_set_cursor(unsigned int column);
void terminal_write_at(unsigned int column, const char *str);

void terminal_draw_line(unsigned int start_column, const char *str,
                        unsigned int length, unsigned int old_length,
                        unsigned int cursor_position);

#endif
