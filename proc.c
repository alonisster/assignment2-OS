#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
int counter_lock = 0;
extern void forkret(void);
extern void trapret(void);
//invokesigret.S
extern void invoke_sigret_start(void);
extern void invoke_sigret_end(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}




int 
allocpid(void) 
{
  int pid;
  do{
        pid = nextpid;       
  } while (cas(&nextpid,pid,pid+1) == 0);
  return pid;
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

  while (cas(&counter_lock,0,1) == 0);
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  counter_lock=0;
  return 0;

found:
  p->state = EMBRYO;
  counter_lock =0;

  p->pid = allocpid();

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  //cleaning pending signals
  p->pending_signals = 0;
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

  return p;
}




//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
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

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

void copySignalHandlers(struct proc * np, struct proc * curproc){
  for (int i = 0; i < 32; i++)
  {
    np->signal_handlers[i].sa_handler = curproc->signal_handlers[i].sa_handler;   //chek if we need to clone handler XXXX
    np->signal_handlers[i].sigmask = curproc->signal_handlers[i].sigmask;
  }  
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  np->signal_mask = curproc-> signal_mask;
  copySignalHandlers(np, curproc);

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
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
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
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
int
kill(int pid, int signum)
{
  struct proc *p;
  if(signum<0 || signum >31)
      return -1;
  
  acquire(&ptable.lock);
  int signumBit= 1 << signum;  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->pending_signals = p -> pending_signals | signumBit;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;    //didnt find proc

  
  // struct proc *p;

  // acquire(&ptable.lock);
  // for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
  //   if(p->pid == pid){
  //     p->killed = 1;
  //     // Wake process from sleep if necessary.
  //     if(p->state == SLEEPING)
  //       p->state = RUNNABLE;
  //     release(&ptable.lock);
  //     return 0;
  //   }
  // }
  // release(&ptable.lock);
  // return -1;
}

void handleKill(){
    cprintf("%s", "reached handle kill\n");
    struct proc * curproc = myproc();
    curproc->killed = 1;
    //Wake process from sleep if necessary.
    if(curproc->state == SLEEPING)
      curproc->state = RUNNABLE;  
}

void handleStop(){
  cprintf("%s", "reached handlestop\n");

  struct proc * curproc = myproc();
  curproc->is_stopped =1;

  //maybe  changing ignore flag for SIGCONT hadler.   
  // int sigCont_bits = 1 << SIGCONT;
  // while(!(curproc->pending_signals & sigCont_bits) ){
  //   yield();
  // }
  // handleCont();
}

  // Wake process from sleep if necessary.
//   if(myproc->state == SLEEPING)
//     myproc->state = RUNNABLE;  
// }

void handleCont(){
  struct proc * curproc = myproc();
  curproc->is_stopped =0;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

uint sigprocmask(uint mask){
  struct proc *curproc = myproc();
  uint oldmask = curproc -> signal_mask;
  curproc -> signal_mask = mask;
  return oldmask;
}

int sigaction(int signum, struct sigaction * act, struct sigaction * oldact ){
  if(act == NULL || signum == SIGKILL || signum == SIGSTOP)
    return -1;
  struct proc *curproc = myproc();
  if(oldact != NULL){
    oldact->sa_handler = curproc -> signal_handlers[signum].sa_handler;
    oldact -> sigmask = curproc -> signal_handlers[signum].sigmask;
  }
  curproc -> signal_handlers[signum].sa_handler = act->sa_handler;
  curproc -> signal_handlers[signum].sigmask = act ->sigmask;
  return 0;

}

void sigret(){
  struct proc * curproc = myproc();
  memmove(curproc->tf, curproc->user_trap_frame_backup,sizeof(struct trapframe));
  curproc->signal_mask = curproc->signal_mask_backup;
}

void handle_all_signals(){
  struct proc * curproc = myproc();
  if(curproc ==0){
    return;
  }
  // consider adding another variable to continue the loop from last index point if returned after user signal.
  int sig_bits = 1;  
  for (int i = 0; i < 32 && curproc->killed == 0; i++)
  {
    if(sig_bits & curproc->pending_signals){
      // cprintf("%d\n ", i);
      struct proc * curproc = myproc();
      void * handler =  curproc ->signal_handlers[i].sa_handler;
      if(handler == SIG_DFL){
        switch (i)
        {
        case SIGSTOP:
          handleStop();
          curproc->pending_signals -= sig_bits;
          break;
        case SIGCONT:
          if(! (curproc->signal_mask & sig_bits)){
            handleCont();
            curproc->pending_signals -= sig_bits;
          }
          break;
        case SIGKILL:
          handleKill();
          curproc->pending_signals -= sig_bits;
          break;    
        default:
          //if not mentioned to be ignored, we kill the process.
          if(! (curproc->signal_mask & sig_bits)){
            handleKill();
            curproc->pending_signals -= sig_bits;
          }
          break;
        }
      }     // in case handler isn't default
      else if(!curproc->is_stopped){ //if(curproc->is_handling_signal == 0){
        curproc->pending_signals -= sig_bits;
        handle_non_default(i);
        return;
      }      
    }
    sig_bits = sig_bits << 1;
  } 

  if(curproc->is_stopped){
    yield();
  }
  return;
}


void handle_non_default(int signum){
  
  struct proc * curproc = myproc();  
  // curproc->is_handling_signal = 1;
  struct sigaction act = curproc ->signal_handlers[signum];
  void * handler = act.sa_handler;

  //backup the tf and mask
  memmove(curproc->user_trap_frame_backup, curproc->tf, sizeof (*curproc->tf));
  curproc->signal_mask_backup = curproc->signal_mask;

  //changing mask table to ignore specified signals while executing the handler function.
  sigprocmask(act.sigmask);

  //inserting ret adress, arguments and jump to function adress
  int function_mem_size = (uint)&invoke_sigret_end - (uint)&invoke_sigret_start;
  curproc->tf->esp -= function_mem_size;

  //inserting the commands to esp
  memmove((void*)curproc->tf->esp, invoke_sigret_start, (uint) function_mem_size);

  int sigret_sender_adr = curproc->tf->esp;
  curproc->tf->esp -= 4;
  *((int*)(curproc->tf->esp)) = signum;
  curproc->tf->esp -= 4;
  *((int*)(curproc->tf->esp)) = sigret_sender_adr;
  curproc->tf->eip = (uint) handler; // trapret will resume into signal handler
  
  return; 
}



