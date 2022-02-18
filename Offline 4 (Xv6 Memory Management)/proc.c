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
extern void forkret(void);
extern void trapret(void);

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

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  if(DEBUG==1) cprintf("Entry ALLOCPROC [ %d ] \n",p->pid);

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  if(DEBUG==1) cprintf("Inside allocproc : %d\n",p->pid);

  p->timer = 0;
  // for testing
  p->pageFault = 0;

  // create swap when process is created
  if(p->pid > 2)
  {
    // initialize disk portion for each process
    if(DEBUG==1) cprintf("[SWAP FILE] creating swap file ... \n");
    createSwapFile(p);
  }

  // update meta data for all process, not only for pid > 2
  for(int i=0;i<MAX_PYSC_PAGES;i++) p->pagesInPhysicalMemory[i].isUsed = false;
  for(int i=0;i<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;i++) p->pagesInDisc[i].isUsed = false;

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

  if(DEBUG==1) cprintf("DEBUG MODE : ON\n");
  else cprintf("DEBUG MODE : OFF\n");

  if(OPTIMIZATION==1) cprintf("OPTIMIZATION MODE : ON\n");
  else cprintf("OPTIMIZATION MODE : OFF\n");

  if( R_ALGO == 1) cprintf("Page Replacement Algorithm : FIFO\n");
  else if( R_ALGO == 2) cprintf("Page Replacement Algorithm : AGING\n");
  else if( R_ALGO == 3) cprintf("Page Replacement Algorithm : LIFO\n");

  if(DEBUG==1) cprintf("Entry USERINIT\n");
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

  if(DEBUG == 1) procdump();

  if(DEBUG==1){
    cprintf("//////////////////  GROWPROC [ %d ] //////////////////\n",curproc->pid);
  }
  
  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n,0)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  if(DEBUG == 1) procdump();
  curproc->sz = sz;
  switchuvm(curproc);
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
  struct proc *curproc = myproc();

  if(DEBUG==1) {
    cprintf("//////////////////  FORK [ %d ] //////////////////\n",curproc->pid);
    cprintf("pgdir %p\n",curproc->pgdir);
  }

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    cprintf("trouble after copyuvm, new pgdir %p\n",np->pgdir);
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }

  if(DEBUG == 1) cprintf("Inside fork :: new pgdir of forked process : %p\n",np->pgdir);

  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  if(DEBUG==1) cprintf("Inside fork [ %d ] --> [ %d ] \n",curproc->pid,pid);

  // everything related to new forked process is done copying
  // now we will copy the swap file that we created in allocproc for each process

  np->timer = curproc->timer;

  // for testing
  np->pageFault = 0;

  if(curproc->pid > 2)
  {
    
    /**
     * @brief Want to copy the swap file from parent to child
     * 
     * First tried to do this page by page, 
     * but xv6 gets mad when I tried to do so -___- 
     * xv6 tries to fork shell multiple times and fails, after a few attempts succeeds
     * 
     * Took two and a half hours to debug this -____-
     * 
     * The reason maybe that it can't handle such large buffer
     * 
     * Now using PGSIZE/2 and now its not mad -_-
     * 
     */

    char tempPage[PGSIZE/2];
    int curOffset = 0;
    if(DEBUG==1) cprintf("[SWAP FILE] copying swap file ... \n");
    for(int i=0;i<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;i++)
    {
      if(curproc->pagesInDisc[i].isUsed == true)
      {
        int bytesRead = readFromSwapFile(curproc,tempPage,curOffset,PGSIZE/2);
        if(bytesRead == -1) cprintf("Problem occurred in parent swap file reading [inside fork]\n");

        int bytesWrite = writeToSwapFile(np,tempPage,curOffset,bytesRead);
        if(bytesWrite == -1) cprintf("Problem occurred in child swap file writing [inside fork]\n");

        curOffset += bytesRead;
      }
    }
  }

  // here first part of the VM that grows into parent proc is copied. so no need to do anything

  // update meta data for all process, not only for pid > 2

  for(int i=0;i<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;i++)
  {
    // used or not , copy information, EXCEPT ofcourse pgdir which has just been assigned a new one
    np->pagesInDisc[i].isUsed = curproc->pagesInDisc[i].isUsed;
    np->pagesInDisc[i].pageDirectory = np->pgdir; // exception
    np->pagesInDisc[i].virtualAddress = curproc->pagesInDisc[i].virtualAddress;
    np->pagesInDisc[i].timer = curproc->pagesInDisc[i].timer;
    np->pagesInDisc[i].ageCounter = curproc->pagesInDisc[i].ageCounter;
    np->pagesInDisc[i].isImportant = curproc->pagesInDisc[i].isImportant;
  }

  for(int i=0;i<MAX_PYSC_PAGES;i++)
  {
    // same for phyiscal memory pages, copy except pgdir
    np->pagesInPhysicalMemory[i].isUsed = curproc->pagesInPhysicalMemory[i].isUsed;
    np->pagesInPhysicalMemory[i].pageDirectory = np->pgdir; // exception
    np->pagesInPhysicalMemory[i].virtualAddress = curproc->pagesInPhysicalMemory[i].virtualAddress;
    np->pagesInPhysicalMemory[i].timer = curproc->pagesInPhysicalMemory[i].timer;
    np->pagesInPhysicalMemory[i].ageCounter = curproc->pagesInPhysicalMemory[i].ageCounter;
    np->pagesInPhysicalMemory[i].isImportant = curproc->pagesInPhysicalMemory[i].isImportant;
  }

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

  if(DEBUG==1) cprintf("Entry EXIT [ %d ] \n",curproc->pid);

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  // remove swap files
  // for those files that have been created
  if(curproc->pid > 2)
  {
    if(DEBUG==1) cprintf("[SWAP FILE] removing swap file ... \n");
    removeSwapFile(curproc);
  }

  // update meta data for all process, not only for pid > 2
  for(int i=0;i<MAX_PYSC_PAGES;i++) curproc->pagesInPhysicalMemory[i].isUsed = false;
  for(int i=0;i<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;i++) curproc->pagesInDisc[i].isUsed = false;

  procdump();
  
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

  // procdump();

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
  if(DEBUG == 1) cprintf("Inside wait : %d\n",curproc->pid);
  
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

        // proc state has just been marked as UNUSED, see the line above
        // now we will mark the pages unused also

        // update meta data for all process, not only for pid > 2
        for(int i=0;i<MAX_PYSC_PAGES;i++) p->pagesInPhysicalMemory[i].isUsed = false;
        for(int i=0;i<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;i++) p->pagesInDisc[i].isUsed = false;
      

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


// pte_t * mywalk(pde_t *pgdir, const void *va)
// {
//   pde_t *pde;
//   pte_t *pgtab;

//   pde = &pgdir[PDX(va)]; // PDX(va) returns the first 10 bit. pgdir is level 1 page table. So, pgdir[PDX(va)] is level 1 PTE where there is PPN and offset

//   pgtab = (pte_t*)P2V(PTE_ADDR(*pde)); // PTE_ADDR return the first 20 bit or PPN. PPN is converted to VPN for finding 2nd level PTE. pgtab is level 2 page table
//   return &pgtab[PTX(va)]; // PDX(va) re
  
// }
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

  
  cprintf("/////////////////////////////////////////////////////////\n");
  cprintf("//////////////////   PROC DUMP     //////////////////////\n");
  cprintf("/////////////////////////////////////////////////////////\n");

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("<<< %d >>> %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");


    ///////////////////////////////////////////
///////////////////  METHOD 1 //////////////////////////
    /////////////////////////////////////////

    /**
     * 
     * @brief 
     * 
     * TASK 1 (PRINTING PAGE TABLE AND PAGE MAPPING)
     * 
     */

    pde_t *level1PageTable = p->pgdir;

    cprintf("Page tables:\n");
    cprintf("\t Memory Location of Page Directory = %p\n",level1PageTable);

    for(int idxL1=0;idxL1<512;idxL1++) // using 512 because this is the limit before KERNBASE
    {
      pde_t pteL1 = level1PageTable[idxL1];

      if(pteL1 & PTE_U){
        cprintf("\t pdir PTE %d, %d:\n",idxL1,PPN(pteL1)); // PTE_ADDR return PPN or PTE
        cprintf("\t\t memory location of page table = %p\n",PTE_ADDR(pteL1)); 

        pte_t *level2PageTable = P2V(PTE_ADDR(pteL1));
        // cprintf("Test %p\n",level2PageTable);
        for(int idxL2=0;idxL2<1024;idxL2++)
        {
            pde_t pteL2 = level2PageTable[idxL2];

            if(pteL2 & PTE_U){
                cprintf("\t\t ptbl PTE %d, %d, %p\n",idxL2,PPN(pteL2),PTE_ADDR(pteL2)); 
            }

        }
      }
    }

    cprintf("Page mappings:\n");

    for(int idxL1=0;idxL1<512;idxL1++) // using 512 because this is the limit before KERNBASE
    {
      pde_t pteL1 = level1PageTable[idxL1];

      if(pteL1 & PTE_U){

        pte_t *level2PageTable = P2V(PTE_ADDR(pteL1));

        for(int idxL2=0;idxL2<1024;idxL2++)
        {
            pde_t pteL2 = level2PageTable[idxL2];
            if(pteL2 & PTE_U){
                cprintf("%d -------> %d\n\n",idxL1 * 1024 + idxL2,PPN(pteL2)); 
                // cprintf("%d -------> %d\n",(idxL1 << 10) | idxL2,PPN(pteL2)); 
            }
        }
      }
    }

    if(p->pid <= 2) continue;

      ///////////////////////////////////////////
///////////////////  METHOD 2 /////////////////////////
    /////////////////////////////////////////

    cprintf("pgdir %p\n",p->pgdir);
    cprintf("number of page faults %d\n",p->pageFault);
    cprintf(" << RAM >> ... \n");
    for(int i=0;i<MAX_PYSC_PAGES;i++)
    {
      // pte_t *pte = mywalk(p->pagesInPhysicalMemory[i].pageDirectory,(char*)p->pagesInPhysicalMemory[i].virtualAddress);
      
      if(p->pagesInPhysicalMemory[i].isUsed)
      {
        if(p->pagesInPhysicalMemory[i].isImportant) cprintf("Physical page id %d : VPN %d , pgdir %p , timer %d , age %x \t INUSE \t IMPORTANT\n",i,p->pagesInPhysicalMemory[i].virtualAddress>>12,p->pagesInPhysicalMemory[i].pageDirectory,p->pagesInPhysicalMemory[i].timer,p->pagesInPhysicalMemory[i].ageCounter);
        else cprintf("Physical page id %d : VPN %d , pgdir %p , timer %d , age %x \t INUSE\n",i,p->pagesInPhysicalMemory[i].virtualAddress>>12,p->pagesInPhysicalMemory[i].pageDirectory,p->pagesInPhysicalMemory[i].timer,p->pagesInPhysicalMemory[i].ageCounter);
      }
      else
      {
        if(p->pagesInPhysicalMemory[i].isImportant) cprintf("Physical page id %d : VPN %d , pgdir %p , timer %d , age %x \t \t \t IMPORTANT\n",i,p->pagesInPhysicalMemory[i].virtualAddress>>12,p->pagesInPhysicalMemory[i].pageDirectory,p->pagesInPhysicalMemory[i].timer,p->pagesInPhysicalMemory[i].ageCounter);
        else cprintf("Physical page id %d : VPN %d , pgdir %p , timer %d , age %x \n",i,p->pagesInPhysicalMemory[i].virtualAddress>>12,p->pagesInPhysicalMemory[i].pageDirectory,p->pagesInPhysicalMemory[i].timer,p->pagesInPhysicalMemory[i].ageCounter);
      }      
    }

    cprintf(" << DISK >> ... \n");
    for(int i=0;i<MAX_PYSC_PAGES;i++)
    {
      // pte_t *pte = mywalk(p->pagesInPhysicalMemory[i].pageDirectory,(char*)p->pagesInPhysicalMemory[i].virtualAddress);
      
      if(p->pagesInDisc[i].isUsed)
      {
        if(p->pagesInDisc[i].isImportant) cprintf("Disk page id %d : VPN %d , pgdir %p , timer %d \t INUSE \t IMPORTANT\n",i,p->pagesInDisc[i].virtualAddress>>12,p->pagesInDisc[i].pageDirectory,p->pagesInDisc[i].timer);
        else cprintf("Disk page id %d : VPN %d , pgdir %p , timer %d \t INUSE\n",i,p->pagesInDisc[i].virtualAddress>>12,p->pagesInDisc[i].pageDirectory,p->pagesInDisc[i].timer);
      }
      else
      {
        if(p->pagesInDisc[i].isImportant) cprintf("Disk page id %d : VPN %d , pgdir %p , timer %d \t \t \t IMPORTANT\n",i,p->pagesInDisc[i].virtualAddress>>12,p->pagesInDisc[i].pageDirectory,p->pagesInDisc[i].timer);
        else cprintf("Disk page id %d : VPN %d , pgdir %p , timer %d \n",i,p->pagesInDisc[i].virtualAddress>>12,p->pagesInDisc[i].pageDirectory,p->pagesInDisc[i].timer);
      }      
    }

  }
}


void updateAgeForAllProcess()
{
  acquire(&ptable.lock);
  for(struct proc *curproc = ptable.proc; curproc < &ptable.proc[NPROC]; curproc++){

    if(curproc->pid > 2)
    {
      if(curproc->state == UNUSED) continue;
      if(curproc->state == EMBRYO) continue;
      if(curproc->state == ZOMBIE) continue;

      updateAgeForSingleProcess(curproc);
    }
  }
  release(&ptable.lock);
}

