#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "auth.h"
#include "uid.h"

// Demo credentials for this teaching image.
static const struct {
  const char *name;
  char password[AUTH_PASSWORD_MAX + 1];
  int uid;
} accounts[] = {
  {"root", "root-xv6-test", UID_ROOT},
  {"isaac", "isaac-xv6-test", UID_ISAAC},
  {"samir", "samir-xv6-test", UID_SAMIR},
};

uint64
sys_startsession(void)
{
  return kstartsession();
}

uint64
sys_readsecret(void)
{
  uint64 dst;

  argaddr(0, &dst);
  return consolesecret(dst);
}

uint64
sys_authenticate(void)
{
  struct proc *p = myproc();
  char name[AUTH_NAME_MAX + 1];
  char password[AUTH_PASSWORD_MAX + 1] = {0};
  int uid = UID_UNAUTH;

  if (p->uid != UID_UNAUTH || p->login_state != LOGIN_PENDING)
    return -1;

  if (argstr(0, name, sizeof(name)) > 0 &&
      argstr(1, password, sizeof(password)) > 0) {
    for (int i = 0; i < NELEM(accounts); i++) {
      // Compare a fixed number of bytes without stopping at the first mismatch.
      unsigned diff = 0;
      for (int j = 0; j < sizeof(password); j++)
        diff |= (uchar)password[j] ^ (uchar)accounts[i].password[j];
      if (diff == 0 &&
          strncmp(name, accounts[i].name, strlen(accounts[i].name) + 1) == 0)
        uid = accounts[i].uid;
    }
  }
  memset(password, 0, sizeof(password));
  if (uid == UID_UNAUTH)
    return -1;

  p->uid = uid;
  p->login_state = LOGIN_VERIFIED;
  return 0;
}
