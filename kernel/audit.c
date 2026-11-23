#include "types.h"
#include "spinlock.h"
#include "audit.h"

static struct spinlock audit_lock;
static struct audit_event events[AUDIT_CAPACITY];
static uint next_event;
static uint event_count;
static int initialized;

void
auditinit(void)
{
  if (!initialized) {
    initlock(&audit_lock, "audit");
    initialized = 1;
  }
}

void
audit_denied(int uid, int pid, int operation, int inum)
{
  struct audit_event *event;

  if (!initialized)
    auditinit();
  acquire(&audit_lock);
  event = &events[next_event];
  event->pid = pid;
  event->uid = uid;
  event->operation = operation;
  event->inum = inum;
  next_event = (next_event + 1) % AUDIT_CAPACITY;
  if (event_count < AUDIT_CAPACITY)
    event_count++;
  release(&audit_lock);
}

// Copy oldest-to-newest events into a kernel buffer. Returns event count.
int
audit_read(struct audit_event *out, int capacity)
{
  int i, count, start;

  if (!initialized)
    auditinit();
  if (capacity < 0)
    return -1;
  acquire(&audit_lock);
  count = event_count;
  if (count > capacity)
    count = capacity;
  start = (next_event + AUDIT_CAPACITY - event_count) % AUDIT_CAPACITY;
  for (i = 0; i < count; i++)
    out[i] = events[(start + i) % AUDIT_CAPACITY];
  release(&audit_lock);
  return count;
}
