#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

// allocuvm functions
int spaceAvialableInDisk(struct proc* curproc);
int spaceAvialableInMemory(struct proc* curproc);
void updatePhysicalPageMetaData(struct proc* curproc, pde_t *pageDirectory, uint virtualAddress,int isImportant);
int reallocatePageInDisk(struct proc* curproc, pde_t *pageDirectory);

//deallocuvm functions
void removeDiscMetaData(struct proc* curproc, pde_t *pageDirectory, uint virtualAddress);
void removePhysicalPageMetaData(struct proc* curproc, pde_t *pageDirectory, uint virtualAddress);

// +--------10------+-------10-------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |      Index     |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(va) --/ \--- PTX(va) --/

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)]; // PDX(va) returns the first 10 bit. pgdir is level 1 page table. So, pgdir[PDX(va)] is level 1 PTE where there is PPN and offset
  if(*pde & PTE_P){ // not NULL and Present
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde)); // PTE_ADDR return the first 20 bit or PPN. PPN is converted to VPN for finding 2nd level PTE. pgtab is level 2 page table
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)]; // PDX(va) returns the second 10 bit. So, pgtab[PTX(va)] is level 2 PTE where there is PPN and offset
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;
  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);

  if(DEBUG==1) cprintf("Inside inituvm ::: PPN %d \t ---- \t VPN %d\n",V2P(mem)>>12,0>>12);

  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  struct proc *curproc = myproc();

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }

  if(DEBUG==1) cprintf("Inside loaduvm : %d\n",curproc->pid);

  return 0;
}

int getPageNumberNeed(uint sz)
{
  return PGROUNDUP(sz)/PGSIZE;
}

/**
 * @brief allocuvm
 * Allocate page tables and physical memory to grow process from oldsz to
 * newsz, which need not be page aligned.  Returns new size or 0 on error.
 * 
 * Modification made :
 * - added RAM page limit
 * - added disk page limit
 * - if space not anymore allowed inside RAM, paged out to disk
 * - OPTIMIZATION IDEA 1 : one extra modification/optimization I made out of specification is that added "isImportant" flag to page meta data,
 *   this is done to mark code and stack segments as important as it will be needed everytime we want to access,
 *   so i always want to have this in RAM
 * - OPTIMIZATION IDEA 2 : another optimization idea I had was to do kind of a lazy memory allocation. 
 *   when allocuvm is called from growproc to allocate memory, we can just save the metadata of the pages that we want to store in disk and not do page out right now
 *   this will ensure less memory transfer overhead
 *   When I tried to implement this, faced some problems reagrding lazy swap file writing. After debugging for a day, I abandoned the idea.
 *   The problem might be due to some weird property of swap files given, it causes problem when doing random access. 
 * 
 * @param pgdir 
 * @param oldsz 
 * @param newsz 
 * @param isImportant 
 * @return int 
 */
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz,int isImportant)
{
  struct proc *curproc = myproc();

  if(DEBUG == 1) cprintf("::: Allocuvm :::: From %p , Now %p\n",pgdir,curproc->pgdir);

  char *mem;
  uint a;

  if(newsz >= KERNBASE) // can't go above KERNBASE : check Xv6 book figure
    return 0;
  if(newsz < oldsz) // allocuvm only want to increase the user's virtual memory in a specific page directory, here can't increase
    return oldsz;

  if(DEBUG == 1) cprintf("Total page number needed (now): %d\n",getPageNumberNeed(newsz));

  int av1 = spaceAvialableInMemory(curproc);
  int av2 = spaceAvialableInDisk(curproc);

  if(av1 == 0 && av2 == 0){ // no more space allowed, you had enough
    if(DEBUG==1) cprintf("will overflow MAXPAGE (%d)\n",MAX_TOTAL_PAGES);
    return 0; // commented for now
  }

  a = PGROUNDUP(oldsz); // from old size, rounding that up [CEIL] to get the address as multiple of page number : https://stackoverflow.com/questions/43289022/what-do-pgroundup-and-pgrounddown-in-xv6-mean
  int pageCount = 0;
  for(; a < newsz; a += PGSIZE){ // 
    pageCount++;
    
    mem = kalloc(); // return an address of a new, currently unused, page in RAM. If it returns 0, that means there are no available unused pages currently.
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){ // responsible of making the new allocated page to be accessible by the process who uses the given page directory by mapping that page with the next virtual address available in the page directory
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }

    if(DEBUG == 1) cprintf("Inside allocuvm : %d ::: PPN %d \t ---- \t VPN %d\n",curproc->pid,V2P(mem)>>12,a>>12);

    if(curproc->pid > 0) // just to be safe
    {
      if(spaceAvialableInMemory(curproc)){ // still has space
        if(DEBUG == 1) cprintf("<<< allocating page in phyiscal memory >>>\n");
        
        // page has been allocated when kalloc was used, I don't have to do anyting to allocate that
        // now I just need to update the meta data associated with physical memory pages
        updatePhysicalPageMetaData(curproc,pgdir,a,isImportant);
      }
      else{ // overflow in phsical memory page limit

        if(DEBUG == 1) cprintf("<<< reallocating page from phyiscal memory to disk >>>\n");
        
        if(reallocatePageInDisk(curproc,pgdir) != -1)
        {
          // done with transferring to disk. free space avaialble now
          // now, can finally give that
          if(DEBUG==1) cprintf("<<< updating meta data >>>\n");
          updatePhysicalPageMetaData(curproc,pgdir,a,isImportant);
        }
      }
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{

  struct proc *curproc = myproc();

  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){

    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
        
      char *v = P2V(pa);
      kfree(v);

      if(DEBUG==1) cprintf("Inside deallocuvm : %d ::: PPN %d \t ----- \t VPN %d\n",curproc->pid,pa>>12,a>>12);

      // remove from physical memory
      removePhysicalPageMetaData(curproc,pgdir,a);
      *pte = 0;
    }
    else if((*pte & PTE_PG) != 0) // this page is not in memory anymore
    {
      // as this was already deallocated when it was paged out,
      // we only need to update meta data

      // remove from disc
      removeDiscMetaData(curproc,pgdir,a);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;
  
  struct proc *curproc = myproc();

  if(DEBUG==1) cprintf("inside freevm : %d\n",curproc->pid);

  if(pgdir == 0)
    panic("freevm: no pgdir");

  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  struct proc *curproc = myproc();
  if(DEBUG==1) cprintf("::: copyuvm :::: From %p , Now %p\n",pgdir,curproc->pgdir);

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");

    if(*pte & PTE_PG)
    {
      pte_t *pte = walkpgdir(d, (int*)i, 0);
      if (!pte) panic("PTE of swapped page is missing");
      *pte |= PTE_PG;
      *pte &= ~PTE_P;
      *pte &= PTE_FLAGS(*pte); //clear junk physical address
      lcr3(V2P(curproc->pgdir)); //refresh CR3 register
      continue;
    }

    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }

    if(DEBUG==1) cprintf("Inside copyuvm : %d ::: PPN %d \t ---- \t VPN %d\n",curproc->pid,V2P(mem)>>12,i>>12);
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

    ////////////////////////////////////////////////////////////////////////
///////////////////  allocuvm meta data update functions //////////////////////////
    /////////////////////////////////////////////////////////////////////

/**
 * @brief return 0 if no space available in disk
 * 
 * @param curproc 
 * @return int 
 */
int spaceAvialableInDisk(struct proc* curproc)
{
  for(int pageIndex=0;pageIndex<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;pageIndex++)
  {
    if(curproc->pagesInDisc[pageIndex].isUsed == false)
    {
      return 1;
    }
  }
  return 0;
}

/**
 * @brief return 0 if no space available in RAM
 * 
 * @param curproc 
 * @return int 
 */
int spaceAvialableInMemory(struct proc* curproc)
{
  for(int pageIndex=0;pageIndex<MAX_PYSC_PAGES;pageIndex++)
  {
    if(curproc->pagesInPhysicalMemory[pageIndex].isUsed == false)
    {
      return 1;
    }
  }
  return 0;
}

/**
 * @brief updatePhysicalPageMetaData
 *    
 * @param curproc 
 * @param pageDirectory 
 * @param virtualAddress 
 */
void updatePhysicalPageMetaData(struct proc* curproc, pde_t *pageDirectory, uint virtualAddress,int isImportant){
  
  int ppn_id = -1; // this is just a simple 0 based indexing of the MAX_PYSC_PAGES pages stored in ram, don't confuse it with PPN
  for(int pageIndex=0;pageIndex<MAX_PYSC_PAGES;pageIndex++)
  {
    if(curproc->pagesInPhysicalMemory[pageIndex].isUsed == false)
    {
      ppn_id = pageIndex;
      break;
    }
  }

  if(ppn_id != -1)
  {
    // update page (in physical memory) meta data
    curproc->pagesInPhysicalMemory[ppn_id].isUsed = true;
    curproc->pagesInPhysicalMemory[ppn_id].pageDirectory = pageDirectory;
    curproc->pagesInPhysicalMemory[ppn_id].virtualAddress = virtualAddress;
    curproc->pagesInPhysicalMemory[ppn_id].timer = curproc->timer++;
    curproc->pagesInPhysicalMemory[ppn_id].ageCounter = 0;
    curproc->pagesInPhysicalMemory[ppn_id].isImportant = isImportant;
  }
  else
  {
    if(DEBUG==1) cprintf("What the heck are you doing here, you should have already initiated swap!");
  }
}

    ////////////////////////////////////////////////////////////////////////
///////////////////  allocuvm page replacement functions //////////////////////////
    /////////////////////////////////////////////////////////////////////

/**
 * @brief AGING Replacement
 * 
 * @param curproc 
 * @return int 
 */
int getReplacementPageIndexAging(struct proc* curproc)
{
  int retIndex = -1;
  uint minAge = 0xffffffff;

  for(int pageIndex = 0;pageIndex < MAX_PYSC_PAGES;pageIndex++)
  {
    if(curproc->pagesInPhysicalMemory[pageIndex].isUsed == false) continue;

    if(curproc->pagesInPhysicalMemory[pageIndex].ageCounter < minAge)
    {
      minAge = curproc->pagesInPhysicalMemory[pageIndex].ageCounter;
      retIndex = pageIndex;
    }
  }

  return retIndex;
}

/**
 * @brief LIFO Replacement
 * 
 * @param curproc 
 * @return int 
 */
int getReplacementPageIndexLIFO(struct proc* curproc)
{
  int retIndex = -1;
  int maxT = 0;
  for(int pageIndex=0;pageIndex<MAX_PYSC_PAGES;pageIndex++)
  {
    if(curproc->pagesInPhysicalMemory[pageIndex].isUsed == false) continue;

    if(curproc->pagesInPhysicalMemory[pageIndex].timer > maxT)
    {
        maxT = curproc->pagesInPhysicalMemory[pageIndex].timer;
        retIndex = pageIndex;
    }
  }
  return retIndex;
}

/**
 * @brief FIFO Replacement 
 * 
 * @param curproc 
 * @return int 
 */
int getReplacementPageIndexFIFO(struct proc* curproc)
{
  if(OPTIMIZATION == 0)
  {
    int minT = __INT_MAX__;
    int retIndex = -1;

    for(int pageIndex=0;pageIndex<MAX_PYSC_PAGES;pageIndex++)
    {
      if(curproc->pagesInPhysicalMemory[pageIndex].isUsed == false) continue;

      if(curproc->pagesInPhysicalMemory[pageIndex].timer < minT)
      {
        minT = curproc->pagesInPhysicalMemory[pageIndex].timer;
        retIndex = pageIndex;
      }
    }
    return retIndex;
  }
  else if(OPTIMIZATION == 1)
  {
    int minT = __INT_MAX__;
    int retIndex = -1;

    for(int pageIndex=0;pageIndex<MAX_PYSC_PAGES;pageIndex++)
    {
      if(curproc->pagesInPhysicalMemory[pageIndex].isImportant == 1) continue;
      if(curproc->pagesInPhysicalMemory[pageIndex].isUsed == false) continue;

      if(curproc->pagesInPhysicalMemory[pageIndex].timer < minT)
      {
        minT = curproc->pagesInPhysicalMemory[pageIndex].timer;
        retIndex = pageIndex;
      }
    }
    return retIndex;
  }
}

/**
 * @brief Get index of a page to replace from physical memory
 * 
 * @param curproc 
 * @return int 
 */
int getReplacementPageIndexPhyMem(struct proc* curproc){

    if(R_ALGO == 1) return getReplacementPageIndexFIFO(curproc);
    else if(R_ALGO == 2) return getReplacementPageIndexAging(curproc);
    else if(R_ALGO == 3) return getReplacementPageIndexLIFO(curproc);
}

/**
 * @brief Get the physical address associated with a virtual address, given a page directory
 * 
 * @param pageDirectory 
 * @param virtualAddress 
 * @return int 
 */
int getReplacementPageAddressPhyMem(pde_t * pageDirectory,uint virtualAddress){
  pte_t* pte;
  if( (pte = walkpgdir(pageDirectory, (int*)virtualAddress, 0)) == 0) return -1;

  if(*pte & PTE_U) 
  {
    return PTE_ADDR(*pte); // physical address
  }
  else
  {
    if(DEBUG == 1) cprintf("transferring stack guard to disk, will bring it back again when code tries to accesss beyond stack limit\n");
    return PTE_ADDR(*pte); // physical address
  }
}

/**
 * @brief  Get index of a page to replace to in disk
 * 
 * @param curproc 
 * @return int 
 */
int getAvailablePageIndexDisk(struct proc* curproc){

  for(int pageIndex=0;pageIndex<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;pageIndex++)
  {
    if(curproc->pagesInDisc[pageIndex].isUsed == false)
    {
      return pageIndex;
    }
  }
  return -1;
}

/**
 * @brief Clear memory, flags and address associated with the reallocated page
 * 
 * @param curproc 
 * @param pageDirectory 
 * @param virtualAddress 
 * @param pageId 
 * @param pageToBeFreedAddress 
 */
void clearPagePhyMem(struct proc* curproc,pde_t *pageDirectory, uint virtualAddress,int pageId,int pageToBeFreedAddress){
  
  // free physical memory
  char *v = P2V(pageToBeFreedAddress);
  kfree(v);

  // this page is no more in ram
  curproc->pagesInPhysicalMemory[pageId].isUsed = false;

  // look for the address part and free it
  pte_t* pte;
  if( (pte = walkpgdir(pageDirectory, (int*)virtualAddress, 0)) == 0){
    if(DEBUG == 1) cprintf("Can't find address\n");
    return;
  }

  *pte |= PTE_PG; // as moving to disk, setting this flag
  *pte &= ~PTE_P; // this page is no more in RAM, so clearing this flag
  *pte &= PTE_FLAGS(*pte); // also clear physical address associated with that 
  // if(DEBUG==1) cprintf("got upto here without trappppp\n");

  // after you perform a page out operation, the TLB might still hold a reference to its old mapping
  // to refresh the TLB, you need to refresh the CR3 register
  // this is the same as doing switchuvm()
  // the idea is taken from BGU MM spec
  lcr3(V2P(curproc->pgdir)); // refresh CR3 register
}

/**
 * @brief Reallocate from RAM to Disk
 * 
 * @param curproc 
 * @param pageDirectory 
 * @return int 
 */
int reallocatePageInDisk(struct proc* curproc, pde_t *pageDirectory){

  curproc->pageFault++;
  
  // find the page we want to free from RAM
  int pageToBeFreedIndex = getReplacementPageIndexPhyMem(curproc);
  if(DEBUG == 1) cprintf("page to be freed id : %d\n",pageToBeFreedIndex);
  if(pageToBeFreedIndex == -1) return -1;

  // get physical address of the page we want to free
  int pageToBeFreedAddress = getReplacementPageAddressPhyMem(curproc->pagesInPhysicalMemory[pageToBeFreedIndex].pageDirectory,curproc->pagesInPhysicalMemory[pageToBeFreedIndex].virtualAddress);
  if(pageToBeFreedAddress == -1) return -1;

  // disk data
  int pageToBeTransferredToIndex =  getAvailablePageIndexDisk(curproc);
  if(pageToBeTransferredToIndex == -1) return -1;

  // copy from the virtual address of page to be freed , using given function
  if(writeToSwapFile(curproc, (char*)curproc->pagesInPhysicalMemory[pageToBeFreedIndex].virtualAddress, PGSIZE*pageToBeTransferredToIndex, PGSIZE) == -1){
    // if(DEBUG == 1) cprintf("fail to write\n");
    return -1;
  }

  // update page (in disc) meta data
  curproc->pagesInDisc[pageToBeTransferredToIndex].isUsed = true;
  curproc->pagesInDisc[pageToBeTransferredToIndex].pageDirectory = curproc->pagesInPhysicalMemory[pageToBeFreedIndex].pageDirectory;
  curproc->pagesInDisc[pageToBeTransferredToIndex].virtualAddress = curproc->pagesInPhysicalMemory[pageToBeFreedIndex].virtualAddress;
  curproc->pagesInDisc[pageToBeTransferredToIndex].timer = 0;
  curproc->pagesInDisc[pageToBeTransferredToIndex].ageCounter = 0;
  curproc->pagesInDisc[pageToBeTransferredToIndex].isImportant = curproc->pagesInPhysicalMemory[pageToBeFreedIndex].isImportant; // actually will always be 0 anyway if i do optimization

  // now clear from physical memory
  clearPagePhyMem(curproc,curproc->pagesInPhysicalMemory[pageToBeFreedIndex].pageDirectory,curproc->pagesInPhysicalMemory[pageToBeFreedIndex].virtualAddress,pageToBeFreedIndex,pageToBeFreedAddress);
  return 1;
}

    ////////////////////////////////////////////////////////////////////////
///////////////////  deallocuvm meta data update functions //////////////////////////
    /////////////////////////////////////////////////////////////////////

void removeDiscMetaData(struct proc* curproc, pde_t *pageDirectory, uint virtualAddress)
{

  if(DEBUG==1) cprintf("trying to remove if not already pid %d ... pgdir %p \t vpn %d\n",curproc->pid, pageDirectory,virtualAddress>>12);

  for(int i=0;i<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;i++){
    if(curproc->pagesInDisc[i].isUsed == true && curproc->pagesInDisc[i].pageDirectory == pageDirectory && curproc->pagesInDisc[i].virtualAddress == virtualAddress){
      // clear all meta data
      if(DEBUG==1) cprintf("disc mark removed\n");
      curproc->pagesInDisc[i].isUsed = false;
      curproc->pagesInDisc[i].pageDirectory = 0;
      curproc->pagesInDisc[i].virtualAddress = 0;
      
      return;
    }
  }
}

void removePhysicalPageMetaData(struct proc* curproc, pde_t *pageDirectory, uint virtualAddress)
{

  if(DEBUG==1) cprintf("trying to remove if not already pid %d ... pgdir %p \t vpn %d\n",curproc->pid, pageDirectory,virtualAddress>>12);

  for(int i=0;i<MAX_PYSC_PAGES;i++){
    if(curproc->pagesInPhysicalMemory[i].isUsed == true && curproc->pagesInPhysicalMemory[i].pageDirectory == pageDirectory && curproc->pagesInPhysicalMemory[i].virtualAddress == virtualAddress){
      // clear all meta data
      if(DEBUG==1) cprintf("mark removed\n");
      curproc->pagesInPhysicalMemory[i].isUsed = false;
      curproc->pagesInPhysicalMemory[i].pageDirectory = 0;
      curproc->pagesInPhysicalMemory[i].virtualAddress = 0;
      
      return;
    }
  }
}

    ///////////////////////////////////////////////////////////////
///////////////////  TRAP : PAGE FAULT FUNCTIONS //////////////////////////
    ///////////////////////////////////////////////////////////////

/**
 * @brief ensure that the trapped page was mapped and paged out
 * 
 * @param pageDirectory 
 * @param virtualAddress 
 * @return int 
 */
int checkPTForPage(pde_t * pageDirectory,int virtualAddress)
{
  if(DEBUG == 1) cprintf("checking %d\n",virtualAddress);

  pte_t *pte = walkpgdir(pageDirectory,(char *)virtualAddress,0);
  if(!pte) return 0;
  return (*pte & PTE_PG); // if page is in disk, PG flag will be on
}

static char buffer[PGSIZE]; 

/**
 * @brief retrive page when page fault happens
 * 
 * @param CR2 
 * @return int 
 */
int retrievePage(int CR2)
{
  struct proc *curproc = myproc();
  int virtualAddress = PGROUNDDOWN(CR2);
  char* mem = kalloc();
  memset(mem,0,PGSIZE);

  curproc->pageFault++;

  int ppn_id = -1; // this is just a simple 0 based indexing of the MAX_PYSC_PAGES pages stored in ram, don't confuse it with PPN
  for(int pageIndex=0;pageIndex<MAX_PYSC_PAGES;pageIndex++)
  {
    if(curproc->pagesInPhysicalMemory[pageIndex].isUsed == false)
    {
      ppn_id = pageIndex;
      break;
    }
  }

  if(ppn_id != -1) // room available in ram
  {
      // mark the page now as available in RAM
      pte_t *pte = walkpgdir(curproc->pgdir, (int*)virtualAddress, 0);
      if (!pte) panic("PTE of swapped page is missing");
      if (*pte & PTE_P) panic("PAGE IN REMAP!");
      *pte |= PTE_P | PTE_W | PTE_U;      
      *pte &= ~PTE_PG;    								
      *pte |= V2P(mem);

      // refresh TLB, reason already stated above						
      lcr3(V2P(curproc->pgdir));       

      for(int i=0;i<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;i++)
      {
        if(curproc->pagesInDisc[i].virtualAddress == virtualAddress)
        {
          int bytesRead = readFromSwapFile(curproc,(char *)virtualAddress,i*PGSIZE,PGSIZE);
          if(bytesRead == -1) return -1;

          curproc->pagesInPhysicalMemory[ppn_id] = curproc->pagesInDisc[i];
          curproc->pagesInDisc[i].isUsed = false;
          curproc->pagesInPhysicalMemory[ppn_id].timer = curproc->timer++;
          break;
        }
      }

      return 1;
  }
  else // need to swap again
  {
      /** REALLOCATE PAGE IN DISK **/

      // get the page id we want to evict
      int pageToBeFreedIndex = getReplacementPageIndexPhyMem(curproc);
      if(DEBUG == 1) cprintf("page to be freed id : %d\n",pageToBeFreedIndex);
      if(pageToBeFreedIndex == -1) return -1;

      // get physical address of the page we want to free
      int pageToBeFreedAddress = getReplacementPageAddressPhyMem(curproc->pagesInPhysicalMemory[pageToBeFreedIndex].pageDirectory,curproc->pagesInPhysicalMemory[pageToBeFreedIndex].virtualAddress);
      if(pageToBeFreedAddress == -1) return -1;

      // disk data
      int pageToBeTransferredToIndex =  getAvailablePageIndexDisk(curproc);
      if(pageToBeTransferredToIndex == -1) return -1;

      // copy from the virtual address of page to be freed , using given function
      if(writeToSwapFile(curproc, (char*)curproc->pagesInPhysicalMemory[pageToBeFreedIndex].virtualAddress, PGSIZE*pageToBeTransferredToIndex, PGSIZE) == -1){
        return -1;
      }

      // update page (in disc) meta data
      curproc->pagesInDisc[pageToBeTransferredToIndex].isUsed = true;
      curproc->pagesInDisc[pageToBeTransferredToIndex].pageDirectory = curproc->pagesInPhysicalMemory[pageToBeFreedIndex].pageDirectory;
      curproc->pagesInDisc[pageToBeTransferredToIndex].virtualAddress = curproc->pagesInPhysicalMemory[pageToBeFreedIndex].virtualAddress;
      curproc->pagesInDisc[pageToBeTransferredToIndex].timer = 0;

      // now clear from physical memory
      clearPagePhyMem(curproc,curproc->pagesInPhysicalMemory[pageToBeFreedIndex].pageDirectory,curproc->pagesInPhysicalMemory[pageToBeFreedIndex].virtualAddress,pageToBeFreedIndex,pageToBeFreedAddress);

      // mark the page now as available in RAM
      pte_t *pte = walkpgdir(curproc->pgdir, (int*)virtualAddress, 0);
      if (!pte) panic("PTE of swapped page is missing");
      if (*pte & PTE_P) panic("PAGE IN REMAP!");
      *pte |= PTE_P | PTE_W | PTE_U;      
      *pte &= ~PTE_PG;    							
      *pte |= V2P(mem);  	

      // refresh TLB					
      lcr3(V2P(curproc->pgdir));  

      // all set and ready to come back in RAM
      // transfer memory
      // using a buffer beacuase transfering directly sometimes cause problem
      // using a global buffer because declaring such large space locally causes xv6 to panic
      for(int i=0;i<MAX_TOTAL_PAGES-MAX_PYSC_PAGES;i++)
      {
        if(curproc->pagesInDisc[i].virtualAddress == virtualAddress)
        {
          int bytesRead = readFromSwapFile(curproc,buffer,i*PGSIZE,PGSIZE);
          if(bytesRead == -1) return -1;

          curproc->pagesInPhysicalMemory[pageToBeFreedIndex] = curproc->pagesInDisc[i];
          curproc->pagesInPhysicalMemory[pageToBeFreedIndex].timer = curproc->timer++;
          curproc->pagesInDisc[i].isUsed = false;
          break;
        }
      }

      // transfer from buffer to memory
      memmove(mem, buffer, PGSIZE);

      return 1;
  }

}

    /////////////////////////////////////////////////////
///////////////////  AGING TIMER UPDATE //////////////////////////
    /////////////////////////////////////////////////////

/**
 * @brief Aging algorithm in tanenbum says update in time interrupt
 * 
 * @param curproc 
 */
void updateAgeForSingleProcess(struct proc *curproc)
{
  for(int i=0;i<MAX_PYSC_PAGES;i++)
  {
    if(curproc->pagesInPhysicalMemory[i].isUsed == true)
    {
      // find pte
      pte_t *pte = walkpgdir(curproc->pagesInPhysicalMemory[i].pageDirectory,(char*)curproc->pagesInPhysicalMemory[i].virtualAddress,0);
      // check if access bit is on
      if(*pte & PTE_A)
      {
        *pte &= ~PTE_A; // clear PTE_A , forgot to do this -___- , need to be manually cleared

        uint prevAge = curproc->pagesInPhysicalMemory[i].ageCounter;
        
        // right shift and update MSB with access bit
        prevAge = prevAge >> 1;
        prevAge = prevAge | (1<<31); 

        curproc->pagesInPhysicalMemory[i].ageCounter = prevAge;
      }
    }
  }
}