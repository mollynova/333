#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#ifdef CS333_P2
#include "uproc.h"
#endif

#ifdef CS333_P3P4
struct StateLists {
  struct proc* ready[MAXPRIO+1];
  struct proc* readyTail[MAXPRIO+1];
  struct proc* free;
  struct proc* freeTail;
  struct proc* sleep;
  struct proc* sleepTail;
  struct proc* zombie;
  struct proc* zombieTail;
  struct proc* running;
  struct proc* runningTail;
  struct proc* embryo;
  struct proc* embryoTail;
};
#endif

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
#ifdef CS333_P3P4
  struct StateLists pLists;
  uint PromoteAtTime;
#endif
} ptable;

static struct proc *initproc;

int nextpid = 1;
#ifdef CS333_P2
int nextuid = 1;
int nextgid = 1;
#endif
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

#ifdef CS333_P2
int setProcs(uint max, struct uproc * table);
#endif

#ifdef CS333_P3P4
static void initProcessLists(void);
static void initFreeList(void);
static int stateListAdd(struct proc** head, struct proc** tail, struct proc* p);
static int stateListRemove(struct proc** head, struct proc** tail, struct proc* p);
static void assertState(struct proc* p, enum procstate state);
//static void assertSuccess(struct proc* p, enum procstate state);
#endif


void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}


//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;
#ifdef CS333_P3P4
 // cprintf("\ncalling allocproc()\n");
  acquire(&ptable.lock);
  // Search free list for unused process
  for(p = ptable.pLists.free; p != 0; p = p->next){
    if(p->state == UNUSED)
      // remove process from free list
      if(stateListRemove(&ptable.pLists.free, &ptable.pLists.freeTail, p) < 0){
        release(&ptable.lock);
        panic("Could not remove process from FREE list");
      }
      // assert that process state is indeed 'unused' after its been removed from list
      assertState(p, UNUSED);
      p->budget = DEF_BUDGET;
      p->priority = 0;
      p->next = NULL;
      goto Found;
   }
  release(&ptable.lock);
  return 0;
Found:

  p->state = EMBRYO;
  // Add to EMBRYO list
  if(stateListAdd(&ptable.pLists.embryo, &ptable.pLists.embryoTail, p) < 0){
    release(&ptable.lock);
    panic("Could not add process to EMBRYO list");
  }
  // assert that p's state is indeed embryo, else panic
  assertState(p, EMBRYO);
#else
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;
found:

  p->state = EMBRYO;
#endif
  p->pid = nextpid++;
#ifdef CS333_P2
  p->uid = UID;
  p->gid = GID;
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
#endif
  release(&ptable.lock);
  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
#ifdef CS333_P3P4
    acquire(&ptable.lock);
    if(stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryoTail, p) < 0){
      panic("allocproc(): failed to remove process from EMBRYO list");
    }
    assertState(p, EMBRYO);
    p->state = UNUSED;
    if(stateListAdd(&ptable.pLists.free, &ptable.pLists.freeTail, p) < 0){
      panic("allocproc(): failed to add process to UNUSED list");
    }
    assertState(p, UNUSED);
    release(&ptable.lock);
#else
    p->state = UNUSED;
#endif
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
#ifdef CS333_P1
  p->start_ticks = ticks;
#endif
  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
#ifdef CS333_P3P4
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  release(&ptable.lock);
#endif
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p;

  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
#ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
  p->uid = UID;
  p->gid = GID;
#endif
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");
#ifdef CS333_P3P4
  // attempt to remove process from embryo list
  p->priority = 0;
  acquire(&ptable.lock);
  if(stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryoTail, p) < 0){
    // if process isn't in embryo list, release lock and panic
    release(&ptable.lock);
    panic("Could not remove process from EMBRYO list");
  }
  // assert that process's state is EMBRYO
  assertState(p, EMBRYO);
  // change process's state to RUNNABLE
  p->state = RUNNABLE;
/*
  // add process to ready list- its the first process so we can just point ready
  // to it instead of using the 'add' function. its FIFO so we point tail to it too
  if(stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail, p) < 0){
     release(&ptable.lock);
     panic("Could not add process to RUNNABLE list");
  }
*/
  ptable.pLists.ready[0] = p;
  ptable.pLists.readyTail[0] = p;
  release(&ptable.lock);
  // assert that its state is RUNNABLE
  assertState(p, RUNNABLE);
  p->next = NULL;
  p->budget = DEF_BUDGET;
#else
  p->state = RUNNABLE;
#endif
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
#ifdef CS333_P2
  np->uid = proc->uid;
  np->gid = proc->gid;
#endif
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
#ifdef CS333_P3P4

  acquire(&ptable.lock);
  // attempt to remove np from EMBRYO list
  if(stateListRemove(&ptable.pLists.embryo, &ptable.pLists.embryoTail, np) < 0){
    // if process isnt in EMBRYO list, release lock and panic
    release(&ptable.lock);
    panic("fork(): couldn't remove process from EMBRYO list");
  }
  // assert that np's state is EMBRYO
  assertState(np, EMBRYO);
  // change np's state to RUNNABLE
  np->state = RUNNABLE;
  // add np to 'ready' list
  if(stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail, np) < 0){
    // if failed to add process to 'ready' list, release lock and panic
    release(&ptable.lock);
    panic("fork(): couldn't add process to READY list");
  }
  // assert that np's state is RUNNABLE
  assertState(np, RUNNABLE);
  release(&ptable.lock);
#else
  // standard state transition
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
#endif
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifndef CS333_P3P4
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
  struct proc *e, *s, *z, *re, *ru;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  // proc is the running process, this for loop is looking for its children
  // we have to go through all of the lists EXCEPT for UNUSED looking for parents
  // ZOMBIES list
  for(z = ptable.pLists.zombie; z != 0; z = z->next){
    if(z->parent == proc){
      z->parent = initproc;
      wakeup1(initproc);
    }
  }
  // RUNNING list
  for(ru = ptable.pLists.running; ru != 0; ru = ru->next){
    if(ru->parent == proc){
      ru->parent = initproc;
    }
  }
  // READY list
  for(re = ptable.pLists.ready; re != 0; re = re->next){
    if(re->parent == proc){
      re->parent = initproc;
    }
  }
  // EMBRYO list
  for(e = ptable.pLists.embryo; e != 0; e = e->next){
    if(e->parent == proc){
      e->parent = initproc;
    }
  }
  // SLEEPING list
  for(s = ptable.pLists.sleep; s != 0; s = s->next){
    if(s->parent == proc){
      s->parent = initproc;
    }
  }

  // Jump into the scheduler, never to return.
  // proc will be RUNNING

  // remove proc from RUNNING list, if it fails panic and msg
  if(stateListRemove(&ptable.pLists.running, &ptable.pLists.runningTail, proc) < 0){
    panic("exit(): failed to remove process from RUNNING list");
  }
  // assert that proc's state is RUNNING
  assertState(proc, RUNNING);
  // change proc's state to ZOMBIE
  proc->state = ZOMBIE;
  // add proc to ZOMBIE list
  if(stateListAdd(&ptable.pLists.zombie, &ptable.pLists.zombieTail, proc) < 0){
    panic("exit(): failed to add process to ZOMBIE list");
  }
  // assert that proc's state is ZOMBIE
  assertState(proc, ZOMBIE);
//  release(&ptable.lock);
  sched();
  panic("zombie exit");
}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifndef CS333_P3P4
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
  struct proc *e, *re, *ru, *s, *z;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // embryo list
    havekids = 0;
    for(e = ptable.pLists.embryo; e != 0; e = e->next){
      if(e->parent != proc)
        continue;
      havekids = 1;
    }
    // ready list
    for(re = ptable.pLists.ready; re != 0; re = re->next){
      if(re->parent != proc)
        continue;
      havekids = 1;
    }
    // running list
    for(ru = ptable.pLists.running; ru != 0; ru = ru->next){
      if(ru->parent != proc)
        continue;
      havekids = 1;
    }
    // sleeping list
    for(s = ptable.pLists.sleep; s != 0; s = s->next){
      if(s->parent != proc)
        continue;
      havekids = 1;
    }
    // zombie list
    for(z = ptable.pLists.zombie; z != 0; z = z->next){
      if(z->parent != proc)
        continue;
      havekids = 1;

      pid = z->pid;
      kfree(z->kstack);
      z->kstack = 0;
      freevm(z->pgdir);
      // attempt to remove process from zombie list
      if(stateListRemove(&ptable.pLists.zombie, &ptable.pLists.zombieTail, z) < 0){
        panic("wait(): failed to remove process from ZOMBIE list");
      }
      // assert that process state is ZOMBIE
      assertState(z, ZOMBIE);
      // change process state to UNUSED
      z->state = UNUSED;
      // attempt to add process to UNUSED list
      if(stateListAdd(&ptable.pLists.free, &ptable.pLists.freeTail, z) < 0){
        panic("wait(): failed to add process to UNUSED list");
      }
      // assert that process state is now UNUSED
      assertState(z, UNUSED);

      z->pid = 0;
      z->parent = 0;
      z->name[0] = 0;
      z->killed = 0;
      release(&ptable.lock);
      return pid;
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifndef CS333_P3P4
// original xv6 scheduler. Use if CS333_P3P4 NOT defined.
void
scheduler(void)
{
  struct proc *p;
  int idle;  // for checking if processor is idle

  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      idle = 0;  // not idle this timeslice
      proc = p;
      switchuvm(p);
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}

#else
void
scheduler(void)
{

  struct proc *p;
  int idle;  // for checking if processor is idle


  for(;;){
    // Enable interrupts on this processor.
    sti();

    idle = 1;  // assume idle unless we schedule a process
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
#ifndef CS333_P3_ONLY
    // if its time to adjust priorities
    if(ticks >= ptable.PromoteAtTime ){
      // Promote all

      // reset PromoteAtTime value
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
    }

    // go through ready queues starting at 0 up to MAXPRIO looking for a proc to run
    for(int i = 0; i < (MAXPRIO + 1); ++i){
      p = ptable.pLists.ready[i];
      if(p){
        idle = 0;
	proc = p;
	switchuvm(p);
	p->cpu_ticks_in = ticks;
        if(stateListRemove(&ptable.pLists.ready[i], &ptable.pLists.readyTail[i], p) < 0){
	  release(&ptable.lock);
	  panic("scheduler() P4: could not remove process from ready queue");
	}
	assertState(p, RUNNABLE);
	p->state = RUNNING;
	if(stateListAdd(&ptable.pLists.running, &ptable.pLists.runningTail, p) < 0){
	  release(&ptable.lock);
	  panic("scheduler() P4: could not add process to running list");
	}
	assertState(p, RUNNING);
	swtch(&cpu->scheduler, proc->context);
	switchkvm();
	proc = 0;
	goto BREAK;
      }
    }
    BREAK:
    // if there is a process at the head of the ready list
#else
    p = ptable.pLists.ready;
    if(p){
      idle = 0;
      proc = p;
      switchuvm(p);
      p->cpu_ticks_in = ticks;
      p = ptable.pLists.ready;
      if(stateListRemove(&ptable.pLists.ready, &ptable.pLists.readyTail, p) < 0){
        release(&ptable.lock);
        panic("scheduler(): could not remove process from ready list");
      }
      assertState(p, RUNNABLE);

      p->state = RUNNING;

      if(stateListAdd(&ptable.pLists.running, &ptable.pLists.runningTail, p) < 0){
        release(&ptable.lock);
        panic("scheduler(): could not add process to running list");
      }
      // assert p is now RUNNING, panic if not
      assertState(p, RUNNING);
      p = ptable.pLists.ready;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();
      // Process is done running for now.
      proc = 0;
    }
#endif
    release(&ptable.lock);
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{

  //cprintf("\ncalling sched\n");
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;

#ifdef CS333_P2
  proc->cpu_ticks_total += ticks - proc->cpu_ticks_in;
#endif

#ifdef CS333_P3P4
// check to see if proc->budget >= DEF_BUDGET
// if so, demote its priority before context switch
  if(proc->budget >= DEF_BUDGET){
    if(proc->priority != 0){
      proc->priority -= 1;
    }
  }
#endif
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
#ifndef CS333_P3P4
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
#else
  acquire(&ptable.lock);
  // attempt to remove process from "RUNNING" list
  if(stateListRemove(&ptable.pLists.running, &ptable.pLists.runningTail, proc) < 0){
    panic("yield(): failed to remove process from RUNNING list");
  }
  // assert that process state is RUNNING
  assertState(proc, RUNNING);
  // change process state to RUNNABLE
  proc->state = RUNNABLE;
  // attempt to add process to READY list
  if(stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail, proc) < 0){
    panic("yield(): failed to add process to READY list");
  }
  // assert that process state is now RUNNABLE
  assertState(proc, RUNNABLE);
  sched();
  release(&ptable.lock);
#endif
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// 2016/12/28: ticklock removed from xv6. sleep() changed to
// accept a NULL lock to accommodate.
void
sleep(void *chan, struct spinlock *lk)
{

  if(proc == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){
    acquire(&ptable.lock);
    if (lk) release(lk);
  }

  // Go to sleep.
#ifndef CS333_P3P4
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();
#else
  // search for proc in RUNNING list
  // if we don't find it, panic

  if(stateListRemove(&ptable.pLists.running, &ptable.pLists.runningTail, proc) < 0){
    panic("sleep(): couldn't find process in RUNNING list");
  }
  // if we do find it, remove it from RUNNING list
  // assert that proc's state is RUNNING
  assertState(proc, RUNNING);
  // change its state to SLEEPING
  proc->chan = chan;
  proc->state = SLEEPING;
  // add it to sleep list
  if(stateListAdd(&ptable.pLists.sleep, &ptable.pLists.sleepTail, proc) < 0){
    panic("sleep(): couldn't add process to SLEEPING list");
  }
  // assert its state is SLEEPING
  assertState(proc, SLEEPING);
  sched();
#endif
  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

//PAGEBREAK!
#ifndef CS333_P3P4
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.pLists.sleep; p != 0; p = p->next){
    // remove from sleep list
    if(p->chan == chan){
      if(stateListRemove(&ptable.pLists.sleep, &ptable.pLists.sleepTail, p) < 0){
         release(&ptable.lock);
         panic("wakeup1(): failed to remove process from SLEEPING list");
      }
      // assert that state is sleep
      assertState(p, SLEEPING);
      // change state to RUNNABLE
      p->state = RUNNABLE;
      // add to RUNNABLE list
      if(stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail, p) < 0){
        release(&ptable.lock);
        panic("wakeup1(): failed to add process to RUNNABLE list");
      }
      // assert that its state is now RUNNABLE
      assertState(p, RUNNABLE);
    }
  }
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifndef CS333_P3P4
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  //cprintf("\ncalling kill\n");
  // go through ALL of the lists EXCEPT for UNUSED, kill process if its pid matches pid
  // if it's sleeping, wake it up
  struct proc *z, *ru, *re, *s, *e;
  acquire(&ptable.lock);
  // ZOMBIE list
  for(z = ptable.pLists.zombie; z != 0; z = z->next){
    if(z->pid == pid){
      z->killed = 1;
      release(&ptable.lock);
      return 0;
    }
  }

  // RUNNING list
  for(ru = ptable.pLists.running; ru != 0; ru = ru->next){
    if(ru->pid == pid){
      ru->killed = 1;
      release(&ptable.lock);
      return 0;
    }
  }
  // READY list
  for(re = ptable.pLists.ready; re != 0; re = re->next){
    if(re->pid == pid){
      re->killed = 1;
      release(&ptable.lock);
      return 0;
    }
  }
  // EMBRYO list
  for(e = ptable.pLists.embryo; e != 0; e = e->next){
    if(e->pid == pid){
      e->killed = 1;
      release(&ptable.lock);
      return 0;
    }
  }
  // SLEEPING list
  for(s = ptable.pLists.sleep; s != 0; s = s->next){
    if(s->pid == pid){
      s->killed = 1;
      // remove process from SLEEPING list
      if(stateListRemove(&ptable.pLists.sleep, &ptable.pLists.sleepTail, s) < 0){
        panic("kill(): failed to remove process from SLEEPING list");
      }
      // assert that process's state is SLEEPING
      assertState(s, SLEEPING);
      // change process state to RUNNABLE
      s->state = RUNNABLE;
      // add process to RUNNABLE list
      if(stateListAdd(&ptable.pLists.ready, &ptable.pLists.readyTail, s) < 0){
        panic("kill(): failed to add process to RUNNABLE list");
      }
      // assert that process state is now RUNNABLE
      assertState(s, RUNNABLE);
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif

#ifdef CS333_P2
static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
};

void
print_ticks(uint pticks)
{
    int secs = pticks/1000;
    int tenths = (pticks/100)%10;
    int hundredths = (pticks/10)%10;
    int thousandths = (pticks % 10);

    cprintf("%d.%d%d%d\t", secs, tenths, hundredths, thousandths);
}

void
print_cpu(uint total)
{
  int secs = total/1000;
  int tenths = (total/100)%10;
  int hundredths = (total/10)%10;
  int thousandths = (total % 10);

  cprintf("%d.%d%d%d\t", secs, tenths, hundredths, thousandths);
}
#endif
//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  #ifdef CS333_P1_NOT_NOW
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  cprintf("PID\tState\tName\tElapsed\tPCs\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    uint pticks = ticks - p->start_ticks;

    cprintf("%d\t%s\t%s\t", p->pid, state, p->name);
    print_ticks(pticks);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
  #endif
  #ifdef CS333_P2
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  cprintf("\nPID\tName\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\tPCs\n");
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    uint pticks = ticks - p->start_ticks;

    // start printing ^p data
    cprintf("%d\t%s\t%d\t%d\t", p->pid, p->name, p->uid, p->gid);
    if(p->pid == 1)
      cprintf("1\t");
    else
      cprintf("%d\t", p->parent->pid);
    // print 'elapsed'
    print_ticks(pticks);
    // print 'cpu'
    print_cpu(p->cpu_ticks_total);
    // continue printing ^p data
    cprintf("%s\t%d\t", state, p->sz);

    // print PCs
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
  #endif
}

#ifdef CS333_P3P4
static int
stateListAdd(struct proc** head, struct proc** tail, struct proc* p)
{
  if (*head == 0) {
    *head = p;
    *tail = p;
    p->next = 0;
  } else {
    (*tail)->next = p;
    *tail = (*tail)->next;
    (*tail)->next = 0;
  }

  return 0;
}

static int
stateListRemove(struct proc** head, struct proc** tail, struct proc* p)
{
  if (*head == 0 || *tail == 0 || p == 0) {
    return -1;
  }

  struct proc* current = *head;
  struct proc* previous = 0;

  if (current == p) {
    *head = (*head)->next;
    return 0;
  }

  while(current) {
    if (current == p) {
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found, hit eject.
  if (current == 0) {
    return -1;
  }

  // Process found. Set the appropriate next pointer.
  if (current == *tail) {
    *tail = previous;
    (*tail)->next = 0;
  } else {
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = 0;

  return 0;
}

static void
initProcessLists(void) {
#ifdef CS333_P3_ONLY
  ptable.pLists.ready = 0;
  ptable.pLists.readyTail = 0;
#else
  for(int i = 0; i < (MAXPRIO + 1); ++i){
    ptable.pLists.ready[i] = 0;
    ptable.pLists.readyTail[i] = 0;
  }
#endif
  ptable.pLists.free = 0;
  ptable.pLists.freeTail = 0;
  ptable.pLists.sleep = 0;
  ptable.pLists.sleepTail = 0;
  ptable.pLists.zombie = 0;
  ptable.pLists.zombieTail = 0;
  ptable.pLists.running = 0;
  ptable.pLists.runningTail = 0;
  ptable.pLists.embryo = 0;
  ptable.pLists.embryoTail = 0;
}

static void
initFreeList(void) {
  if (!holding(&ptable.lock)) {
    panic("acquire the ptable lock before calling initFreeList\n");
  }

  struct proc* p;

  for (p = ptable.proc; p < ptable.proc + NPROC; ++p) {
    p->state = UNUSED;
    stateListAdd(&ptable.pLists.free, &ptable.pLists.freeTail, p);
  }
}

// Function to print PIDs of all processes on READY list
void
ctrlr(void)
{
  struct proc *p;
  cprintf("Ready List Processes:\n");

  acquire(&ptable.lock);
  p = ptable.pLists.ready;

  // if ready list is empty
  if(!p){
    cprintf("No processes in ready list.\n");
  }
  // else if there is one item in ready list
  else if(p != 0 && p->next == 0){
    cprintf("%d\n", p->pid);
  }
  // else if there is more than one item in ready list
  else if(p->next != 0){
    for(p = ptable.pLists.ready; p->next != 0; p = p->next){
      cprintf("%d -> ", p->pid);
    }
    cprintf("%d\n", p->pid);
  }
  release(&ptable.lock);
}

// Function to print the number of processes on the free list
void
ctrlf(void)
{
  struct proc *p;
  uint count = 0;
  acquire(&ptable.lock);
  p = ptable.pLists.free;
  if(!p){
    cprintf("Free List Size: 0 processes\n");
  } else {
    for(p = ptable.pLists.free; p != 0; p = p->next){
      ++count;
    }
    cprintf("Free List Size: %d processes\n", count);
  }
  release(&ptable.lock);
}

// Function to print PIDs of all processes on the SLEEP list
void
ctrls(void)
{
  struct proc *p;
  cprintf("Sleep List Processes:\n");

  acquire(&ptable.lock);
  p = ptable.pLists.sleep;

  // if ready list is empty
  if(!p){
    cprintf("No processes in sleep list.\n");
  }
  // else if there is one item in ready list
  else if(p != 0 && p->next == 0){
    cprintf("%d\n", p->pid);
  }
  // else if there is more than one item in ready list
  else if(p->next != 0){
    for(p = ptable.pLists.sleep; p->next != 0; p = p->next){
      cprintf("%d -> ", p->pid);
    }
    cprintf("%d\n", p->pid);
  }
  release(&ptable.lock);
}

// Function to print PIDs and PPIDs of all processes on ZOMBIE list
void
ctrlz(void)
{
  struct proc *p;
  cprintf("Zombie List Processes:\n");

  acquire(&ptable.lock);
  p = ptable.pLists.zombie;

  // if ready list is empty
  if(!p){
    cprintf("No processes in zombie list.\n");
  }
  // else if there is one item in ready list
  else if(p != 0 && p->next == 0){
    if(p->pid == 1){
      cprintf("(%d, 1)\n", p->pid);
    }
    else {
      cprintf("(%d, %d)\n", p->pid, p->parent->pid);
    }
  }
  // else if there is more than one item in ready list
  else if(p->next != 0){
    for(p = ptable.pLists.zombie; p->next != 0; p = p->next){
      if(p->pid == 1){
        cprintf("(%d, 1) -> ", p->pid);
      }
      else {
        cprintf("(%d, %d) -> ", p->pid, p->parent->pid);
      }
    }
    if(p->pid == 1){
      cprintf("(%d, 1)\n", p->pid);
    }
    else{
      cprintf("(%d, %d)\n", p->pid, p->parent->pid);
    }
  }
  release(&ptable.lock);
}
#endif

#ifdef CS333_P2
int
setProcs(uint max, struct uproc * table)
{
 // char* state;
  struct proc *p;
  int i = 0;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(2 <= p->state && p->state <= 4){
      if(i == max) continue;
      table[i].pid = p->pid;
      table[i].uid = p->uid;
      table[i].gid = p->gid;

      if(p->pid == 1)
         table[i].ppid = 1;
      else
         table[i].ppid = p->parent->pid;

      table[i].elapsed_ticks = ticks - p->start_ticks;
      table[i].size = p->sz;
      table[i].CPU_total_ticks = p->cpu_ticks_total;

      strncpy(table[i].state, states[p->state], STRMAX);
      strncpy(table[i].name, p->name, STRMAX);

      ++i;
    } else {
      continue;
    }
  }
  release(&ptable.lock);
  return i;
}
#endif

#ifdef CS333_P3P4
static void
assertState(struct proc* p, enum procstate state)
{
  if(p->state != state){
    cprintf("\nERROR: Process intended state is: %d, Actual state is %d\n", state, p->state);
    panic("Goodbye");
  }
  return;
}
#endif
