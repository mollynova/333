#ifdef CS333_P2
#define MAX 16
#include "types.h"
#include "user.h"
#include "uproc.h"

void
print_ticks(uint pticks)
{
    int secs = pticks/1000;
    int tenths = (pticks/100)%10;
    int hundredths = (pticks/10)%10;
    int thousandths = (pticks % 10);

    printf(1, "%d.%d%d%d\t", secs, tenths, hundredths, thousandths);
}

void
print_cpu(uint total)
{
  int secs = total/1000;
  int tenths = (total/100)%10;
  int hundredths = (total/10)%10;
  int thousandths = (total % 10);

  printf(1, "%d.%d%d%d\t", secs, tenths, hundredths, thousandths);
}

int
main(void)
{
  printf(1, "PID\tName\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\n");

  struct uproc* table  = (struct uproc*)malloc(MAX * sizeof(struct uproc));
  int ret = getprocs(MAX, table);
  /*if(ret) {
    printf(2,"Error: date call failed. %s at line %d\n",
      __FILE__, __LINE__);
      exit();
  }*/
  for(int i = 0; i < ret; ++i){
    uint Uid = table[i].uid;
    uint Pid = table[i].pid;
    uint Gid = table[i].gid;
    uint Ppid = table[i].ppid;
    uint Elapsed_ticks = table[i].elapsed_ticks;
    uint Size = table[i].size;
    printf(1, "%d\t%s\t%d\t%d\t%d\t", Pid, table[i].name, Uid, Gid, Ppid);
    print_ticks(Elapsed_ticks);
    print_cpu(table[i].CPU_total_ticks);
    printf(1, "%s\t%d\n", table[i].state, Size);
  }
  free(table);
  exit();
}
#endif
