#ifdef CS333_P5
#include "types.h"
#include "user.h"
int
main(int argc, char *argv[])
{
  int ret = chgrp(argv[2], atoi(argv[1]));
  if(ret < 0)
    printf(2, "Error: chgrp call failed. %s at line %d\n", __FILE__, __LINE__);
  exit();
}

#endif
