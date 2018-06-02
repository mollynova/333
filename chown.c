#ifdef CS333_P5
#include "types.h"
#include "user.h"
//#include "sysfile.c"
//#include "fs.h"
//#include "stat.h"

int
main(int argc, char *argv[])
{
//  int own = argv[2];
  //printf(1, "argv[0]: %s, argv[1]: %s, argv[2]: %d\n", argv[0], argv[1], atoi(argv[2]));
  //printf(1, "Calling chown like:\n chown(%s, %d)\n", argv[2], atoi(argv[1]));
  //int ret = chown(atoi(argv[1]), argv[2]);
  int ret = chown(argv[2], atoi(argv[1]));
  if(ret < 0){
    printf(2, "Error: chown call failed. %s at line %d\n", __FILE__, __LINE__);
    exit();
  }
  exit();
}

#endif
