#ifndef XV6_AUDIT_H
#define XV6_AUDIT_H

#define AUDIT_CAPACITY 64

enum audit_operation {
  AUDIT_READ = 1,
  AUDIT_WRITE,
  AUDIT_EXEC,
  AUDIT_SEARCH,
  AUDIT_CREATE,
  AUDIT_DELETE,
  AUDIT_LINK,
};

struct audit_event {
  int pid;
  int uid;
  int operation;
  int inum;
};

void audit_denied(int uid, int pid, int operation, int inum);
int audit_read(struct audit_event *events, int capacity);

#endif
