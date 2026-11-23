#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static char *
operation_name(int operation)
{
  switch (operation) {
  case 1: return "read";
  case 2: return "write";
  case 3: return "exec";
  case 4: return "search";
  case 5: return "create";
  case 6: return "delete";
  case 7: return "link";
  default: return "unknown";
  }
}

int
main(void)
{
  struct audit_event events[64];
  int n, i;

  n = auditread(events, 64);
  if (n < 0) {
    fprintf(2, "audit: access denied or invalid request\n");
    exit(1);
  }
  for (i = 0; i < n; i++)
    printf("pid=%d uid=%d op=%s inum=%d denied\n", events[i].pid,
           events[i].uid, operation_name(events[i].operation), events[i].inum);
  exit(0);
}
