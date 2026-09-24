#include "process.h"

static pid_t next_pid = 1;

process_t *process_create(void) {
  static process_t process;
  process.pid = next_pid++;
  process.state = PROCESS_NEW;
  process.address_space = 0;

  return &process;
}
