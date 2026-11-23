#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

char *
fmtname(char *path)
{
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  buf[sizeof(buf) - 1] = '\0';
  return buf;
}

static int longform;

void
show(char *path, struct stat *st)
{
  if (!longform) {
    printf("%s %d %d %d\n", fmtname(path), st->type, st->ino, (int)st->size);
    return;
  }
  char bits[7];
  char *names = "rwxrwx";
  int masks[] = {0400, 0200, 0100, 4, 2, 1};
  for (int i = 0; i < 6; i++)
    bits[i] = st->mode & masks[i] ? names[i] : '-';
  bits[6] = 0;
  printf("%s %s uid=%d mode=0%d%d%d ino=%d size=%d\n", fmtname(path),
         bits, st->uid, (st->mode >> 6) & 7, (st->mode >> 3) & 7,
         st->mode & 7, st->ino, (int)st->size);
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, O_RDONLY)) < 0) {
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {
  case T_DEVICE:
  case T_FILE:
    show(path, &st);
    break;

  case T_DIR:
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if (stat(buf, &st) < 0) {
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      show(buf, &st);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  i = 1;
  if (argc > 1 && strcmp(argv[1], "-l") == 0) {
    longform = 1;
    i++;
  }
  if (argc == i) {
    ls(".");
    exit(0);
  }
  for (; i < argc; i++)
    ls(argv[i]);
  exit(0);
}
