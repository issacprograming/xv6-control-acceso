//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "auth.h"

#define BACKSPACE 0x100       // erase the last output character
#define C(x)      ((x) - '@') // Control-x

//
// send one character to the uart, but don't use
// interrupts or sleep(). safe to be called from
// interrupts, e.g. by printk and to echo input
// characters.
//
void
consputc(int c)
{
  if (c == BACKSPACE) {
    // if the user typed backspace, overwrite with a space.
    uartputc_sync('\b');
    uartputc_sync(' ');
    uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

struct {
  struct spinlock lock;

  // input circular buffer
#define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE];
  uint r; // Read index
  uint w; // Write index
  uint e; // Edit index
  int session;
  int hidden;
  int writers;
} cons;

// Called outside process locks, before publishing the next login process.
void
consolesession(int session)
{
  acquire(&cons.lock);
  cons.session = session;
  cons.hidden = 0;
  memset(cons.buf, 0, sizeof(cons.buf));
  cons.r = cons.w = cons.e = 0;
  wakeup(&cons.r);
  while (cons.writers) {
    sleep_prepare(&cons.writers);
    release(&cons.lock);
    sleep();
    acquire(&cons.lock);
  }
  release(&cons.lock);
}

static int
consoleactive(struct proc *p)
{
  return cons.session != 0 && p->session == cons.session;
}

//
// user write() system calls to the console go here.
// uses sleep() and UART interrupts.
//
int
consolewrite(int user_src, uint64 src, int n)
{
  char buf[32]; // move batches from user space to uart.
  int i = 0;
  struct proc *p = myproc();

  while (i < n) {
    int nn = sizeof(buf);
    if (nn > n - i)
      nn = n - i;

    if (either_copyin(buf, user_src, src + i, nn) == -1)
      return i;

    acquire(&cons.lock);
    if (!consoleactive(p) && !(p->session_manager && cons.session == 0)) {
      release(&cons.lock);
      return i ? i : -1;
    }
    cons.writers++;
    release(&cons.lock);

    uartwrite(buf, nn);
    i += nn;

    acquire(&cons.lock);
    if (--cons.writers == 0)
      wakeup(&cons.writers);
    release(&cons.lock);
  }

  return i;
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dst indicates whether dst is a user
// or kernel address.
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;
  struct proc *p = myproc();

  target = n;
  acquire(&cons.lock);
  while (n > 0) {
    // wait until interrupt handler has put some
    // input into cons.buffer.
    for (;;) {
      if (!consoleactive(p) || killed(p)) {
        release(&cons.lock);
        return -1;
      }
      if (cons.r != cons.w)
        break;
      sleep_prepare(&cons.r);
      release(&cons.lock);
      sleep();
      acquire(&cons.lock);
    }

    c = cons.buf[cons.r % INPUT_BUF_SIZE];

    if (c == C('D')) { // end-of-file
      // Save ^D if this read already returned part of the line.
      if (n == target)
        cons.r++;
      break;
    }
    if (cons.hidden)
      cons.buf[cons.r % INPUT_BUF_SIZE] = 0;
    cons.r++;

    // copy the input byte to the user-space buffer.
    cbuf = c;
    if (either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if (c == '\n') {
      // a whole line has arrived, return to
      // the user-level read().
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}

// Read one password directly from the active console. Keep echo off while
// draining an overlong line, and restore it before returning on any path.
int
consolesecret(uint64 dst)
{
  struct proc *p = myproc();
  char password[AUTH_PASSWORD_MAX + 1];
  int n = 0, result = -1;
  char c = 0;

  acquire(&cons.lock);
  if (!consoleactive(p) || p->login_state != LOGIN_PENDING) {
    release(&cons.lock);
    return -1;
  }
  memset(cons.buf, 0, sizeof(cons.buf));
  cons.r = cons.w = cons.e = 0;
  cons.hidden = 1;
  for (const char *s = "Contrasena: "; *s; s++)
    consputc(*s);
  release(&cons.lock);

  while (consoleread(0, (uint64)&c, 1) == 1) {
    if (c == '\n') {
      if (n <= AUTH_PASSWORD_MAX) {
        password[n] = 0;
        if (copyout(p->pagetable, p->sz, dst, password, n + 1) == 0)
          result = n;
      }
      break;
    }
    if (n < AUTH_PASSWORD_MAX)
      password[n++] = c;
    else
      n = AUTH_PASSWORD_MAX + 1;
  }

  acquire(&cons.lock);
  memset(cons.buf, 0, sizeof(cons.buf));
  cons.r = cons.w = cons.e = 0;
  cons.hidden = 0;
  consputc('\n');
  release(&cons.lock);
  memset(password, 0, sizeof(password));
  return result;
}

//
// the console input interrupt handler.
// uartintr() calls this for each input character.
// do erase/kill processing, append to cons.buf,
// wake up consoleread() if a whole line has arrived.
//
void
consoleintr(int c)
{
  acquire(&cons.lock);

  switch (c) {
  case C('P'): // Print process list.
    procdump();
    break;
  case C('U'): // Kill line.
    while (cons.e != cons.w &&
           cons.buf[(cons.e - 1) % INPUT_BUF_SIZE] != '\n') {
      cons.e--;
      if (!cons.hidden)
        consputc(BACKSPACE);
    }
    break;
  case C('H'): // Backspace
  case '\x7f': // Delete key
    if (cons.e != cons.w) {
      cons.e--;
      if (!cons.hidden)
        consputc(BACKSPACE);
    }
    break;
  default:
    if (c != 0 && cons.e - cons.r < INPUT_BUF_SIZE) {
      c = (c == '\r') ? '\n' : c;

      // echo back to the user.
      if (!cons.hidden)
        consputc(c);

      // store for consumption by consoleread().
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

      if (c == '\n' || c == C('D') || cons.e - cons.r == INPUT_BUF_SIZE) {
        // wake up consoleread() if a whole line (or end-of-file)
        // has arrived.
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }

  release(&cons.lock);
}

void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();

  // connect read and write system calls
  // to consoleread and consolewrite.
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}
