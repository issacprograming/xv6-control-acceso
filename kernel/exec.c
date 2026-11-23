#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"
#include "auth.h"
#include "uid.h"
#include "stat.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "permissions.h"
#include "audit.h"

struct execimage {
  const char *name;
  const char *data;
  uint64 size;
};

extern const struct execimage session_programs[];

static int loadseg(pde_t *, uint64, struct inode *, const struct execimage *,
                   uint64, uint64);

static const struct execimage *
sessionimage(const char *path)
{
  if (*path == '/')
    path++;
  for (const struct execimage *image = session_programs; image->name; image++) {
    if (strncmp(path, image->name, strlen(image->name) + 1) == 0)
      return image;
  }
  return 0;
}

static int
execread(struct inode *ip, const struct execimage *image, uint64 dst,
         uint64 off, uint n)
{
  if (image) {
    if (off > image->size || n > image->size - off)
      return -1;
    memmove((void *)dst, image->data + off, n);
    return n;
  }
  if (off > (uint)-1)
    return -1;
  return readi(ip, 0, dst, off, n);
}

// map ELF permissions to PTE permission bits.
int
flags2perm(int flags)
{
  int perm = 0;
  if (flags & 0x1)
    perm = PTE_X;
  if (flags & 0x2)
    perm |= PTE_W;
  return perm;
}

//
// the implementation of the exec() system call
//
static int
loadexec(char *path, char **argv, const struct execimage *image)
{
  char *s, *last;
  int i;
  uint64 off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip = 0;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();

  if (!image) {
    begin_op();
    if ((ip = namei(path)) == 0) {
      end_op();
      return -1;
    }
    ilock(ip);
    if (ip->type != T_FILE || check_access(ip, PERM_X, AUDIT_EXEC) < 0)
      goto bad;
  }

  // Read the ELF header.
  if (execread(ip, image, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;

  // Is this really an ELF file?
  if (elf.magic != ELF_MAGIC)
    goto bad;

  if ((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
    if (execread(ip, image, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if (ph.type != ELF_PROG_LOAD)
      continue;
    if (ph.memsz < ph.filesz)
      goto bad;
    if (ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if (ph.vaddr % PGSIZE != 0)
      goto bad;
    uint64 sz1;
    if ((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz,
                        flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;
    if (loadseg(pagetable, ph.vaddr, ip, image, ph.off, ph.filesz) < 0)
      goto bad;
  }
  if (ip) {
    iunlockput(ip);
    end_op();
    ip = 0;
  }

  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate some pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the rest as the user stack.
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if ((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK + 1) * PGSIZE, PTE_W)) ==
      0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz - (USERSTACK + 1) * PGSIZE);
  sp = sz;
  stackbase = sp - USERSTACK * PGSIZE;

  // Copy argument strings into new stack, remember their
  // addresses in ustack[].
  for (argc = 0; argv[argc]; argc++) {
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if (sp < stackbase)
      goto bad;
    if (copyout(pagetable, sz, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push a copy of ustack[], the array of argv[] pointers.
  sp -= (argc + 1) * sizeof(uint64);
  sp -= sp % 16;
  if (sp < stackbase)
    goto bad;
  if (copyout(pagetable, sz, sp, (char *)ustack, (argc + 1) * sizeof(uint64)) <
      0)
    goto bad;

  // a0 and a1 contain arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for (last = s = path; *s; s++)
    if (*s == '/')
      last = s + 1;
  safestrcpy(p->name, last, sizeof(p->name));

  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry; // initial program counter = ulib.c:start()
  p->trapframe->sp = sp;         // initial stack pointer
  p->login_state = LOGIN_NONE;
  proc_freepagetable(oldpagetable, oldsz);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

bad:
  if (pagetable)
    proc_freepagetable(pagetable, sz);
  if (ip) {
    iunlockput(ip);
    end_op();
  }
  return -1;
}

int
kbootexec(void)
{
  const struct execimage *image = sessionimage("/init");
  if (!image)
    return -1;
  return loadexec("/init", (char *[]){"/init", 0}, image);
}

int
kexec(char *path, char **argv)
{
  struct proc *p = myproc();
  const struct execimage *image = 0;

  if (p->uid == UID_UNAUTH || p->session_manager ||
      p->login_state == LOGIN_PENDING)
    return -1;

  if (p->login_state == LOGIN_VERIFIED && strncmp(path, "/sh", 4) != 0)
    return -1;

  if (p->login_state == LOGIN_VERIFIED || p->uid == UID_ROOT) {
    image = sessionimage(path);
    if (!image)
      return -1;
  }
  return loadexec(path, argv, image);
}

// Load an ELF program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip,
        const struct execimage *image, uint64 offset, uint64 sz)
{
  uint64 i;
  uint n;
  uint64 pa;

  for (i = 0; i < sz; i += PGSIZE) {
    pa = walkaddr(pagetable, va + i);
    if (pa == 0)
      panic("loadseg: address should exist");
    if (sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if (offset + i < offset ||
        execread(ip, image, (uint64)pa, offset + i, n) != n)
      return -1;
  }

  return 0;
}
