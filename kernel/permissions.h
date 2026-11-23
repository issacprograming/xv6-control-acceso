#ifndef XV6_PERMISSIONS_H
#define XV6_PERMISSIONS_H

#define ROOT_UID 0
#define PERM_R 4
#define PERM_W 2
#define PERM_X 1
#define DEFAULT_FILE_MODE 0644
#define DEFAULT_DIR_MODE 0755

// No groups: the middle octal digit is zero or mirrors "others" for
// compatibility with the agreed 0644/0755/0777 examples. It never grants access.
static inline int
valid_file_mode(int mode)
{
  int group = (mode >> 3) & 7;
  return mode >= 0 && (mode & ~0777) == 0 &&
         (group == 0 || group == (mode & 7));
}

#endif
