#ifdef CS333_P5
#include "types.h"
#include "user.h"
//#include "ulib.c"
#include "stat.h"
#include "fcntl.h"
#include "x86.h"
int
main(int argc, char *argv[])
{
  // i could have also stored atoi(argv[1]) in 'check' and done it with base 10, but I figured I'd stick with the octal
  // so that it's clear we're working with an octal number
  int check = atoo(argv[1]);

  int thou = check / (8*8*8);
  int hund = check / 64;
  if(hund > 7){
    hund -= 8;
  }
  int ten = (check - (thou*(8*8*8)) - (hund*64)) / 8;
  int one = (check - (thou*(8*8*8)) - (hund*(8*8)) - (ten*8));

  if((thou != 0 && thou != 1) || hund < 0 || hund > 7 || ten < 0 || ten > 7 || one < 0 || one > 7){
    printf(2, "Error: chmod call failed. Invalid octal\n");
    exit();
  }

  int ret = chmod(argv[2], check);
  if(ret < 0)
    printf(2, "Error: chmod call failed. %s at line %d\n", __FILE__, __LINE__);
  exit();
}

#endif
