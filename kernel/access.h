#ifndef XV6_ACCESS_H
#define XV6_ACCESS_H

#include "audit.h"

// requested is one of PERM_R, PERM_W or PERM_X. Caller holds ip->lock.
int check_access(struct inode *ip, int requested, int operation);

#endif
