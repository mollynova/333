#ifdef CS333_P2
#include "types.h"
#include "user.h"
int
main(int argc, char **argv)
{
  uint start_time = uptime();
  int secs, tenths, hundredths;

  if(argc == 1){
    secs = 0;
    tenths = 0;
    hundredths = 0;
    goto WHEE;
   }

  int childid = fork();

  if(childid == 0){
      exec(argv[1], (argv + 1) );
      printf(1, "Whoopsie, cookie, you did a boo boo");
    } else {
    //wait(childid);
    wait();
  }
  uint end_time = uptime();
  uint total_time = end_time - start_time;

  secs = total_time/1000;
  tenths = (total_time/100)%10;
  hundredths = (total_time/10)%10;

  printf(1, "%s ran in ", argv[1]);
  WHEE:
  printf(1, "ran in ", argv[1]);
  printf(1, "%d.%d%d seconds.\n", secs, tenths, hundredths);

  exit();
}

#endif
