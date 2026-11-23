// Host-side layout helper (x86_64 and RV64 share these integer/pointer sizes).
// Not linked into xv6 and never installed in its image.
#include <stddef.h>
#include <stdio.h>
#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/riscv.h"
#include "kernel/spinlock.h"
#include "kernel/proc.h"
int main(void) {
  printf("%zu %zu %zu %zu %zu\n", sizeof(struct proc),
         offsetof(struct proc, uid), offsetof(struct proc, name),
         offsetof(struct proc, pid), offsetof(struct proc, state));
}
