// Samir's reproducible module/integration cases. No identity-changing syscall.
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static char *file = "/home/samir/p3.txt";
static char *victim = "/home/samir/p7dir/victim";
static void
require(int ok, char *message)
{
  if (!ok) {
    printf("FAIL: %s\n", message);
    exit(1);
  }
  printf("PASS: %s\n", message);
}
static void
metadata(char *path, int uid, int mode)
{
  struct stat st;
  require(stat(path, &st) == 0, "stat path");
  printf("metadata uid=%d mode=0%d%d%d\n", st.uid, (st.mode >> 6) & 7,
         (st.mode >> 3) & 7, st.mode & 7);
  require(st.uid == uid && st.mode == mode, "owner and persistent mode");
}
int
main(int argc, char **argv)
{
  int fd;
  char buf[8] = {0};
  require(argc == 2, "one test stage supplied");
  printf("permtest stage=%s uid=%d\n", argv[1], getuid());
  if (!strcmp(argv[1], "p3")) {
    require(getuid() == 1002, "Samir identity");
    unlink(file);
    fd = open(file, O_CREATE | O_RDWR);
    require(fd >= 0, "P3 create own file");
    require(write(fd, "samir\n", 6) == 6, "P3 write own file");
    close(fd);
    metadata(file, 1002, 0644);
    require(chmod(file, 0600) == 0, "P3 private mode");
    fd = open(file, O_RDONLY);
    require(fd >= 0 && read(fd, buf, 6) == 6 && !strcmp(buf, "samir\n"),
            "P3 read own private file");
    close(fd);
    metadata(file, 1002, 0600);
  } else if (!strcmp(argv[1], "p5-owner")) {
    require(getuid() == 1002, "owner identity");
    require(chmod("/home/samir", 0755) == 0, "P5 searchable parent for other UID");
    require(chmod(file, 0644) == 0, "P5 owner chmod allowed");
    metadata(file, 1002, 0644);
    require(chmod(file, -1) == -1, "P5 reject negative mode");
    require(chmod(file, 01000) == -1, "P5 reject special bits");
    require(chmod(file, 0670) == -1, "P5 reject independent group rights");
    require(chmod("/no-such-file", 0600) == -1, "P5 reject missing path");
    require(chmod((char *)~(uint64)0, 0600) == -1, "P5 reject invalid pointer");
    metadata(file, 1002, 0644);
  } else if (!strcmp(argv[1], "p5-other")) {
    require(getuid() == 1001, "non-owner identity");
    require(chmod(file, 0600) == -1, "P5 non-owner chmod denied");
    fd = open(file, O_RDONLY);
    require(fd >= 0, "P5 other UID reads mode 0644");
    close(fd);
    require(open(file, O_WRONLY) == -1, "P5 other UID cannot write mode 0644");
  } else if (!strcmp(argv[1], "p5-root")) {
    require(getuid() == 0, "root identity");
    metadata(file, 1002, 0644);
    require(chmod(file, 0600) == 0, "P5 root chmod allowed");
    metadata(file, 1002, 0600);
  } else if (!strcmp(argv[1], "persist")) {
    metadata(file, 1002, 0600);
    fd = open(file, O_RDONLY);
    require(fd >= 0 && read(fd, buf, 6) == 6 && !strcmp(buf, "samir\n"),
            "P8b file content survived reboot");
    close(fd);
  } else if (!strcmp(argv[1], "p7-setup")) {
    require(getuid() == 1002, "owner identity");
    mkdir("/home/samir/p7dir");
    require(chmod("/home/samir/p7dir", 0700) == 0, "P7 restricted directory");
    fd = open(victim, O_CREATE | O_WRONLY);
    require(fd >= 0, "P7 victim created");
    close(fd);
  } else if (!strcmp(argv[1], "p7-other")) {
    require(getuid() == 1001, "non-owner identity");
    require(unlink(victim) == -1, "P7 unlink protected file denied");
  } else if (!strcmp(argv[1], "p7-owner")) {
    require(getuid() == 1002, "owner identity");
    require(unlink(victim) == 0, "P7 owner unlink allowed");
  } else {
    require(0, "known stage");
  }
  printf("DONE: %s\n", argv[1]);
  exit(0);
}
