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
  int check = atoi(argv[1]);

  int thou = check / 1000;
  int hund = (check - (thou*1000)) / 100;
  int ten = (check - (thou*1000) - (hund*100)) / 10;
  int one = (check - (thou*1000) - (hund*100) - (ten*10));

  //if((thou != 0 && thou != 1) || hund < 0 || hund > 7 || ten < 0 || ten > 7 || one < 0 || one > 7){
  if((thou > 7) || (hund > 7) || (ten > 7) || (one > 7)){
    printf(2, "Error: chmod call failed. Invalid octal\n");
    exit();
  }
  int ret = chmod(argv[2], atoo(argv[1]));
  if(ret == -1){
    printf(2, "Error: chmod call failed. %s at line %d\n", __FILE__, __LINE__);
  } else if(ret == -2){
    printf(2, "Error: chmod call failed. Invalid octal\n");
  }
  exit();
}

#endif
