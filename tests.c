#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "syscall.h"
//#include "proc.h"
//#include "param.h"
//#include "stat.h"
//#include "fs.h"
//#include "traps.h"
//#include "memlayout.h"

void
testuidgid(void)
{
  uint uid, gid, ppid, pid;

  pid = getpid();
  printf(2, "Current PID is: %d\n", pid);

  uid = getuid();
  printf(2, "Current UID is: %d\n", uid);
  printf(2, "Setting UID to 100\n");
  setuid(100);
  uid = getuid();
  printf(2, "Current UID is: %d\n", uid);

  gid = getgid();
  printf(2, "Current GID is: %d\n", gid);
  printf(2, "Setting GID to 100\n");
  setgid(100);
  gid = getgid();
  printf(2, "Current GID is: %d\n", gid);

  ppid = getppid();
  printf(2, "My parent process is: %d\n", ppid);
  printf(2, "Done!\n");

//  return 0;
}

int
main(int argc, char *argv[])
{
   testuidgid();
   exit();
}
#endif