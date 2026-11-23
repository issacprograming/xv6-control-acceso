#include "types.h"
#include "proc.h"
#include "file.h"
#include "permissions.h"
#include "uid.h"
#include "audit.h"
#include "access.h"
#include "defs.h"

void
auditinit(void);

int
check_access(struct inode *ip, int requested, int operation)
{
  int uid = get_current_uid();
  int allowed;

  if (uid == UID_ROOT)
    return 0;
  if (uid == UID_UNAUTH ||
      (requested != PERM_R && requested != PERM_W && requested != PERM_X))
    allowed = 0;
  else if (uid == ip->uid)
    allowed = (((ip->mode >> 6) & requested) == requested);
  else
    allowed = ((ip->mode & requested) == requested);
  if (!allowed) {
    struct proc *p = myproc();
    audit_denied(uid, p->pid, operation, ip->inum);
    return -1;
  }
  return 0;
}
