#include "kernel/types.h"
#include "kernel/auth.h"
#include "user/user.h"

static int
readname(char *name)
{
  char c;
  int n = 0;

  while (read(0, &c, 1) == 1) {
    if (c == '\n') {
      if (n == 0 || n > AUTH_NAME_MAX)
        return -2;
      name[n] = 0;
      return n;
    }
    if (n < AUTH_NAME_MAX)
      name[n++] = c;
    else
      n = AUTH_NAME_MAX + 1;
  }
  return -1;
}

void
login(void)
{
  char name[AUTH_NAME_MAX + 1];
  char password[AUTH_PASSWORD_MAX + 1];

  for (;;) {
    printf("Usuario: ");
    int n = readname(name);
    if (n == -1)
      exit(0);

    int result = -1;
    if (n > 0 && readsecret(password) > 0)
      result = authenticate(name, password);
    memset(password, 0, sizeof(password));

    if (result == 0) {
      exec("/sh", (char *[]){"sh", 0});
      fprintf(2, "login: no se pudo iniciar la shell\n");
      exit(1);
    }

    fprintf(2, "Credenciales invalidas\n");
    pause(10);
  }
}
