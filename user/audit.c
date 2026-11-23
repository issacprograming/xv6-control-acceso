#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static char *
operation_name(int operation)
{
  switch (operation) {
  case AUDIT_READ: return "read";
  case AUDIT_WRITE: return "write";
  case AUDIT_EXEC: return "exec";
  case AUDIT_SEARCH: return "search";
  case AUDIT_CREATE: return "create";
  case AUDIT_DELETE: return "delete";
  case AUDIT_LINK: return "link";
  case AUDIT_CHMOD: return "chmod";
  case AUDIT_MKNOD: return "mknod";
  case AUDIT_QUERY: return "auditread";
  default: return "unknown";
  }
}

int
main(void)
{
  struct audit_event events[AUDIT_CAPACITY];
  int n, i;

  n = auditread(events, AUDIT_CAPACITY);
  if (n < 0) {
    fprintf(2, "audit: access denied or invalid request\n");
    exit(1);
  }
  for (i = 0; i < n; i++)
    printf("pid=%d uid=%d op=%s inum=%d denied\n", events[i].pid,
           events[i].uid, operation_name(events[i].operation), events[i].inum);
  exit(0);
}
