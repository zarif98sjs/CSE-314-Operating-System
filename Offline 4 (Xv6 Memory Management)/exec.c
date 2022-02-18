#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  if(DEBUG==1)
  {
      cprintf("//////////////////  EXEC [ %d ]  //////////////////\n",curproc->pid);
      cprintf("path is %s , pgdir is %p\n",path,curproc->pgdir);
  }
  
  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz,1)) == 0) // code segment -> important
      goto bad;
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  if(DEBUG==1) cprintf("Inside exec (BEFORE 2 page allocuvm): %d\n",curproc->pid);
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + PGSIZE,0)) == 0) // stack segment -> first one is guard, not important
    goto bad;  
  if((sz = allocuvm(pgdir, sz, sz + PGSIZE,1)) == 0) // stack segment -> important
    goto bad;
  if(DEBUG==1) cprintf("Inside exec (AFTER 2 page allocuvm): %d\n",curproc->pid);
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp;

  if(DEBUG==1) cprintf("Exiting EXEC : %d , pgdir is %p\n",curproc->pid,curproc->pgdir);

  if(curproc->pid > 2)
  {
    if(DEBUG == 1) cprintf("redundant swap remove\n");
    removeSwapFile(curproc);
    createSwapFile(curproc);
  }

  // doing this just to be safe, won't be a problem anyway as meta data will be updated in allocuvm
  // actaully not doing this because some things might not get deleted , maybe ?
  // for(int i=0;i<MAX_PYSC_PAGES;i++)
  // {
  //   curproc->pagesInPhysicalMemory[i].pageDirectory = curproc->pgdir;
  // }

  switchuvm(curproc);
  // cprintf("Inside exec : %d\n",curproc->pid);
  
  // for safety, if deallocuvm is called with another pid
  // for(int i=0;i<MAX_PYSC_PAGES;i++)
  // {
  //   if(curproc->pagesInPhysicalMemory[i].isUsed == false) continue;

  //   if(curproc->pagesInPhysicalMemory[i].pageDirectory == oldpgdir)
  //     curproc->pagesInPhysicalMemory[i].isUsed = false;
  // }

  // for(int i=0;i<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;i++)
  // {
  //   if(curproc->pagesInDisc[i].isUsed == false) continue;

  //   if(curproc->pagesInDisc[i].pageDirectory == oldpgdir)
  //   {
  //     if(DEBUG == 1) cprintf("New mark removed\n");
  //     curproc->pagesInDisc[i].isUsed = false;
  //   }
  // }

  freevm(oldpgdir);

  // procdump();
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}
