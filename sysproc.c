#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "uproc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      return -1;
    }
    sleep(&ticks, (struct spinlock *)0);
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  xticks = ticks;
  return xticks;
}

//Turn of the computer
int
sys_halt(void){
  cprintf("Shutting down ...\n");
  outw( 0x604, 0x0 | 0x2000);
  return 0;
}

#ifdef CS333_P1
int
sys_date(void)
{
  struct rtcdate *d;
  if(argptr(0, (void*)&d, sizeof(struct rtcdate)) < 0)
    return -1;
  cmostime(d);
  return 0;
}
#endif

#ifdef CS333_P2
uint
sys_getuid(void)
{
  return proc->uid;
  //return 0;
}

uint
sys_getgid(void)
{
  return proc->gid;
}

uint
sys_getppid(void)
{
  // if process is init, ppid == uid
  if(proc->pid == 1)
    return 1;
  // otherwise return the pid of the parent
  return proc->parent->pid;
}

int
sys_setuid(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  if(n < 0 || n > 32767)
    return -1;

  proc->uid = n;
  return 0;
}

int
sys_setgid(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  if(n < 0 || n > 32767)
    return -1;
  proc->gid = n;
  return 0;
}

int
sys_getprocs(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  struct uproc *procs;
  if(argptr(1, (void*)&procs, sizeof(struct uproc)) < 0)
    return -1;

  return setProcs(n, procs);

  // call function in proc.h, pass this to it
  // return the number of structs you actually put in the uprocs array
  // cast it to a uint yo
}
#endif

#ifdef CS333_P3P4
int
sys_setpriority(void)
{
  int pid, prio;
  if((argint(0, &pid) <= 0) || (argint(0, &pid) != proc->pid))
    return -1;
  if((argint(1, &prio) < 0) || (argint(1, &prio) > MAXPRIO))
    return -1;
  proc->priority = prio;
  return 0;
}
#endif
