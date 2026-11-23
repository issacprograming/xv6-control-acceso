#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  int uid = getuid();
  char uidarg[9];
  uint bits = uid;
  for (int i = 7; i >= 0; i--) {
    uidarg[i] = "0123456789abcdef"[bits & 15];
    bits >>= 4;
  }
  uidarg[8] = 0;

  if (argc == 3 && strcmp(argv[1], "after-exec") == 0) {
    if (strcmp(argv[2], uidarg) != 0) {
      fprintf(2, "uidtest: UID changed after exec to %d\n", uid);
      exit(1);
    }
    printf("uidtest: exec preserved UID %d\n", uid);
    exit(0);
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

    exec("/uidtest", (char *[]){"uidtest", "after-exec", uidarg, 0});
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
