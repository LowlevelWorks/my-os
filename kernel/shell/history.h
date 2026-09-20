#ifndef HISTORY_H
#define HISTORY_H

#define SHELL_HISTORY_SIZE 16

void history_init(void);

void history_add(const char *command);

void history_reset_position(void);

const char *history_previous(void);
const char *history_next(void);

#endif
