// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/auth.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void login(void) __attribute__((noreturn));

int
main(void)
{
  int pid, wpid;

  if (open("/console", O_RDWR) < 0)
    exit(1);
  dup(0); // stdout
  dup(0); // stderr

  for (;;) {
    pid = startsession();
    if (pid < 0) {
      fprintf(2, "init: no se pudo iniciar la sesion\n");
      if (pid == STARTSESSION_DENIED)
        exit(1);
      pause(10);
      continue;
    }
    if (pid == 0)
      login();

    for (;;) {
      // this call to wait() returns if the session exits,
      // or if a parentless process exits.
      wpid = wait((int *)0);
      if (wpid == pid) {
        // The session ended; start a new login.
        break;
      } else if (wpid < 0) {
        printf("init: wait returned an error\n");
        exit(1);
      } else {
        // it was a parentless process; do nothing.
      }
    }
  }
}
