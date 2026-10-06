#include "sleep.h"

#include "panic/panic.h"
#include "pit.h"

#include "process/scheduler/scheduler.h"
#include "process/thread/thread.h"

#define MAX_SLEEP_ENTRIES 128

typedef struct sleep_entry {
  thread_t *thread;
  uint64_t wakeup_tick;
  struct sleep_entry *next;
} sleep_entry_t;

static sleep_entry_t sleep_entries[MAX_SLEEP_ENTRIES];
static sleep_entry_t *sleep_head;

void sleep_init(void) {
  sleep_head = 0;

  for (uint64_t i = 0; i < MAX_SLEEP_ENTRIES; i++) {
    sleep_entries[i].thread = 0;
    sleep_entries[i].wakeup_tick = 0;
    sleep_entries[i].next = 0;
  }
}

static sleep_entry_t *sleep_entry_alloc(void) {
  for (uint64_t i = 0; i < MAX_SLEEP_ENTRIES; i++) {
    if (sleep_entries[i].thread == 0)
      return &sleep_entries[i];
  }

  return 0;
}

static int tick_reached(uint64_t now, uint64_t deadline) {
  return (int64_t)(now - deadline) >= 0;
}

static int tick_before(uint64_t first, uint64_t second) {
  return (int64_t)(first - second) < 0;
}

static void sleep_list_insert(sleep_entry_t *entry) {
  if (sleep_head == 0 ||
      tick_before(entry->wakeup_tick, sleep_head->wakeup_tick)) {
    entry->next = sleep_head;
    sleep_head = entry;
    return;
  }

  sleep_entry_t *current = sleep_head;

  while (current->next != 0 &&
         !tick_before(entry->wakeup_tick, current->next->wakeup_tick)) {
    current = current->next;
  }

  entry->next = current->next;
  current->next = entry;
}

void thread_sleep(uint64_t ticks) {
  if (ticks == 0)
    return;

  __asm__ volatile("cli");

  thread_t *current = thread_current();

  if (current == 0)
    panic("thread_sleep: no current thread");

  if (current->state != THREAD_RUNNING)
    panic("thread_sleep: thread is not running");

  sleep_entry_t *entry = sleep_entry_alloc();

  if (entry == 0)
    panic("thread_sleep: sleep table full");

  entry->thread = current;
  entry->wakeup_tick = pit_ticks() + ticks;
  entry->next = 0;

  current->state = THREAD_BLOCKED;

  sleep_list_insert(entry);

  scheduler_block_current();

  __asm__ volatile("sti");
}

void sleep_tick(uint64_t current_tick) {
  while (sleep_head != 0 &&
         tick_reached(current_tick, sleep_head->wakeup_tick)) {
    sleep_entry_t *entry = sleep_head;

    sleep_head = entry->next;

    thread_t *thread = entry->thread;

    entry->thread = 0;
    entry->wakeup_tick = 0;
    entry->next = 0;

    if (thread->state != THREAD_BLOCKED)
      panic("sleep_tick: sleeping thread is not blocked");

    scheduler_wake(thread);
  }
}
