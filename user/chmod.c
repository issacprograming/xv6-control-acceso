#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/permissions.h"

int
main(int argc, char **argv)
{
  int mode = 0, n;
  if (argc != 3)
    goto usage;
  n = strlen(argv[1]);
  if (n != 3 && !(n == 4 && argv[1][0] == '0'))
    goto usage;
  for (int i = 0; i < n; i++) {
    if (argv[1][i] < '0' || argv[1][i] > '7')
      goto usage;
    mode = mode * 8 + argv[1][i] - '0';
  }
  if (!valid_file_mode(mode))
    goto usage;
  if (chmod(argv[2], mode) < 0) {
    fprintf(2, "chmod: cannot change %s (owner/root required; check path)\n", argv[2]);
    exit(1);
  }
  exit(0);
usage:
  fprintf(2, "usage: chmod MODE PATH (e.g. 0600, 0644, 0700, 0755)\n");
  exit(1);
}
