#include "kernel/types.h"
#include "kernel/uid.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  int uid = getuid();

  if (argc == 2 && strcmp(argv[1], "after-exec") == 0) {
    if (uid != UID_UNAUTH) {
      fprintf(2, "uidtest: exec expected UID %d, got %d\n", UID_UNAUTH, uid);
      exit(1);
    }
    printf("uidtest: exec preserved UID %d\n", uid);
    exit(0);
  }

  if (uid != UID_UNAUTH) {
    fprintf(2, "uidtest: expected initial UID %d, got %d\n", UID_UNAUTH, uid);
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "uidtest: fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    int child_uid = getuid();
    if (child_uid != uid) {
      fprintf(2, "uidtest: fork expected UID %d, got %d\n", uid, child_uid);
      exit(1);
    }
    printf("uidtest: fork inherited UID %d\n", child_uid);

    exec("/uidtest", (char *[]){"uidtest", "after-exec", 0});
    fprintf(2, "uidtest: exec failed\n");
    exit(1);
  }

  int status;
  if (wait(&status) != pid || status != 0) {
    fprintf(2, "uidtest: child failed\n");
    exit(1);
  }
  printf("uidtest: OK\n");
  exit(0);
}
