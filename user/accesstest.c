// Integrated acceptance checks; all identities come from the real login.
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/auth.h"
#include "user/user.h"

static char *private = "/tmp/privado";
static char *foreign = "/tmp/ajeno";
static void
ok(int condition, char *message)
{
  if (!condition) {
    printf("FAIL: %s\n", message);
    exit(1);
  }
  printf("PASS: %s\n", message);
}
static void
makefile(char *path)
{
  int fd = open(path, O_CREATE | O_WRONLY | O_TRUNC);
  ok(fd >= 0, "create fixture");
  ok(write(fd, "secreto\n", 8) == 8, "write fixture");
  close(fd);
}
int
main(int argc, char **argv)
{
  int status, pid;
  struct stat st;
  struct audit_event events[AUDIT_CAPACITY];
  ok(argc >= 2, "test supplied");
  printf("accesstest %s UID=%d\n", argv[1], getuid());
  if (!strcmp(argv[1], "setup")) {
    ok(getuid() == 1001, "Isaac owns fixtures");
    makefile(private);
    ok(chmod(private, 0600) == 0, "P4 private mode 0600");
    makefile(foreign);
    ok(stat(private, &st) == 0 && st.uid == 1001, "P4 owner UID 1001");
  } else if (!strcmp(argv[1], "p4")) {
    ok(getuid() == 1002, "Samir is not owner");
    ok(open(private, O_RDONLY) == -1, "P4 read denied");
    ok(open(private, O_WRONLY | O_TRUNC) == -1, "P4 truncate denied");
    ok(auditread(events, AUDIT_CAPACITY) == -1, "audit query is root-only");
  } else if (!strcmp(argv[1], "p6")) {
    ok(getuid() == 1002, "normal user");
    pid = fork();
    ok(pid >= 0, "fork for denied exec");
    if (pid == 0) {
      int result = exec("/echo", (char *[]){"echo", "SHOULD_NOT_RUN", 0});
      exit(result == -1 ? 42 : 1);
    }
    ok(wait(&status) == pid && status == 42, "P6 exec without x denied");
  } else if (!strcmp(argv[1], "p7-other")) {
    ok(getuid() == 1002, "non-owner in shared directory");
    ok(unlink(foreign) == -1, "P7 foreign unlink denied despite parent 0777");
  } else if (!strcmp(argv[1], "p7-owner")) {
    ok(getuid() == 1001, "owner in shared directory");
    ok(stat(foreign, &st) == 0 && st.size == 8 && st.uid == 1001,
       "P7 rejected unlink preserved file and owner");
    ok(unlink(foreign) == 0, "P7 owner unlink allowed");
  } else if (!strcmp(argv[1], "audit")) {
    ok(getuid() == 0, "root queries audit");
    int n = auditread(events, AUDIT_CAPACITY), read_seen = 0, exec_seen = 0, delete_seen = 0;
    struct stat bin;
    ok(stat(private, &st) == 0 && stat("/echo", &bin) == 0, "audit object metadata");
    for (int i = 0; i < n; i++) {
      if (events[i].uid != 1002)
        continue;
      if (events[i].operation == AUDIT_READ && events[i].inum == st.ino)
        read_seen = 1;
      if (events[i].operation == AUDIT_EXEC && events[i].inum == bin.ino)
        exec_seen = 1;
      if (events[i].operation == AUDIT_DELETE)
        delete_seen = 1;
    }
    ok(n > 0 && read_seen && exec_seen && delete_seen,
       "P4 P6 P7 audit records match UID and denied objects");
  } else if (!strcmp(argv[1], "ring-fill")) {
    ok(getuid() == 1002, "normal user generates denied events");
    for (int i = 0; i < 66; i++)
      ok(open(private, O_RDONLY) == -1, "denied event");
    printf("RING_PID=%d\n", getpid());
  } else if (!strcmp(argv[1], "ring-check")) {
    ok(getuid() == 0 && argc == 3, "root and expected process supplied");
    int n = auditread(events, AUDIT_CAPACITY);
    ok(n == 64, "audit bounded at 64 after 66 denials");
    ok(stat(private, &st) == 0, "ring object exists");
    for (int i = 0; i < n; i++)
      ok(events[i].uid == 1002 && events[i].pid == atoi(argv[2]) &&
         events[i].operation == AUDIT_READ && events[i].inum == st.ino,
         "ring retained latest generator events");
  } else if (!strcmp(argv[1], "auth")) {
    int uid = getuid();
    ok(uid == 1002, "authenticated normal identity");
    ok(startsession() == STARTSESSION_DENIED, "ordinary process cannot start privileged login");
    ok(authenticate("root", "root-xv6-test") == -1 && getuid() == uid,
       "authenticated process cannot escalate through authenticate");
    ok(mknod("/home/samir/fake", 1, 0) == -1, "normal user cannot create device");
  } else if (!strcmp(argv[1], "fd")) {
    ok(getuid() == 1002, "owner identity");
    char *path = "/home/samir/p3.txt", data[8] = {0};
    int writer = open(path, O_RDWR), reader = open(path, O_RDONLY);
    ok(writer >= 0 && reader >= 0, "descriptors opened before chmod");
    ok(chmod(path, 0000) == 0, "remove all new-open permissions");
    ok(write(writer, "samir\n", 6) == 6, "existing descriptor remains writable");
    ok(read(reader, data, 6) == 6 && !strcmp(data, "samir\n"),
       "existing descriptor remains readable");
    ok(open(path, O_RDONLY) == -1, "new descriptor respects changed mode");
    close(reader);
    close(writer);
    ok(chmod(path, 0600) == 0, "owner restores mode");
  } else if (!strcmp(argv[1], "p7-parent")) {
    ok(getuid() == 1002, "directory owner");
    char *dir = "/home/samir/p7dir", *path = "/home/samir/p7dir/victim";
    ok(chmod(dir, 0500) == 0, "parent has x but not w");
    ok(unlink(path) == -1, "P7 own unlink needs parent w");
    ok(chmod(dir, 0600) == 0, "parent has w but not x");
    ok(unlink(path) == -1, "P7 own unlink needs parent x");
    ok(chmod(dir, 0700) == 0, "restore parent wx");
    ok(stat(path, &st) == 0, "rejected operations preserved victim");
  } else {
    ok(0, "known case");
  }
  printf("DONE: %s\n", argv[1]);
  exit(0);
}
