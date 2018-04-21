#include "ps.c"
#include "types.h"
#include "user.h"

static void
testgetproc(void)
{
  uint uid = getuid();
  printf(1, "Current UID is: %d\n", uid);
  printf(1, "Setting UID to %d\n", nval);
  if (setuid(nval) < 0)
    printf(2, "Error. Invalid UID: %d\n", nval);
  uid = getuid();
  printf(1, "Current UID is: %d\n", uid);
  sleep(5 * TPS);  // now type control-p
}

int
main() {
  testgetproc();
  exit();
}
