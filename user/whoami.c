#include "kernel/types.h"
#include "kernel/uid.h"
#include "user/user.h"

int
main(void)
{
  int uid = getuid();
  const char *name;

  switch (uid) {
  case UID_UNAUTH:
    name = "sin autenticar";
    break;
  case UID_ROOT:
    name = "root";
    break;
  case UID_ISAAC:
    name = "isaac";
    break;
  case UID_SAMIR:
    name = "samir";
    break;
  default:
    name = "desconocido";
    break;
  }

  printf("%s (UID %d)\n", name, uid);
  exit(0);
}
