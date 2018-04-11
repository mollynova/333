#ifdef CS333_P2
#define MAX 16
#include "types.h"
#include "user.h"
#include "proc.c"
#include "uproc.h"
int
main(void)
{
  struct uproc* procs = (struct uproc*)malloc(MAX * sizeof(struct uproc));
  if(getprocs(&procs)) {
    printf(2,"Error: date call failed. %s at line %d\n",
      __FILE__, __LINE__);
      exit();
  }

  printf(1, "PRINT ALL YA SHIT HERE\n");
  exit();
}
#endif
