// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "mm.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 */
int
enlist_vm_freerg_list (struct mm_struct *mm, struct vm_rg_struct rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;
  struct vm_rg_struct *new_rgnode = malloc (sizeof (struct vm_rg_struct));

  if (rg_elmt.rg_start >= rg_elmt.rg_end)
    return -1;

  if (rg_node != NULL)
    {
      new_rgnode->rg_next = rg_node;
    }

  new_rgnode->rg_start = rg_elmt.rg_start;
  new_rgnode->rg_end = rg_elmt.rg_end;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = new_rgnode;

  return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 */
struct vm_area_struct *
get_vma_by_num (struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma = mm->mmap;

  if (mm->mmap == NULL)
    return NULL;

  int vmait = 0;

  while (vmait < vmaid)
    {
      if (pvma == NULL)
        return NULL;

      pvma = pvma->vm_next;
    }

  return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *
get_symrg_byid (struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 */
int
__alloc (struct pcb_t *caller, int vmaid, int rgid, int size,
         uint32_t *alloc_addr)
{
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;

  /* If found free vmrg_area
   * return the area to alloc_addr
   * */
  if (get_free_vmrg_area (caller, vmaid, size, &rgnode) == 0)
    {
      caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
      caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
#ifdef MMDBG
      printf ("\t[ALLOC] PID=%d get free region %lu %lu\n", caller->pid,
              rgnode.rg_start, rgnode.rg_end);
#endif

      *alloc_addr = rgnode.rg_start;

      return 0;
    }

  /*Attempt to increate limit to get space */
  // int inc_sz = PAGING_PAGE_ALIGNSZ (size);
  // int inc_limit_ret

  /* INCREASE THE LIMIT
   * Ascender the sbrk cursor
   * inc_vma_limit(caller, vmaid, inc_sz)
   */
  int stat = inc_vma_limit (caller, vmaid, size, &caller->mm->symrgtbl[rgid]);

  if (stat == -1)
    return -1;

  *alloc_addr = caller->mm->symrgtbl[rgid].rg_start;

  return 0;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int
pg_getpage (struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PAGE_PRESENT (pte))
    { /* Page is not online, make it actively living */
      struct memphy_struct *swpsrc = NULL;
      int vicpgn, swpfpn;
      int vicfpn;
      uint32_t vicpte;
      int swptype
          = 0; /* We only have one swap devices which is the first one */

      int tgtfpn = PAGING_SWP (pte); // The target frame storing our variable

      /* Find and pop victim page out */
      if (find_victim_page (caller->mm, &vicpgn) != 0)
        return -1;              /* Invalid page access */

      vicpte = mm->pgd[vicpgn]; /* Get victim's page entry */

      vicfpn = GETVAL (vicpte, PAGING_PTE_FPN_MASK,
                       PAGING_PTE_FPN_LOBIT); /* Get victim's frame number */

      /* Get free frame in MEMSWP */
      if (MEMPHY_get_freefp (caller->active_mswp, &swpfpn) != 0)
        return -1;
      else
        swpsrc = caller->active_mswp;

      /* Do swap frame from MEMRAM to MEMSWP and vice versa*/
      /* Copy victim frame to swap */
      __swap_cp_page (caller->mram, vicfpn, swpsrc, swpfpn);
      /* Copy target frame from swap to mem */
      __swap_cp_page (caller->active_mswp, tgtfpn, caller->mram, vicfpn);

      /* Update pte of victim to swap */
      pte_set_swap (&mm->pgd[vicpgn], swptype, vicfpn);

      /* Update the target page online status */
      pte_set_fpn (&pte, tgtfpn);

      enlist_pgn_node (&caller->mm->fifo_pgn, pgn);
    }

  *fpn = GETVAL (mm->pgd[pgn], PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

  return 0;
}
/*pg_putfree - free value inside the MRAM free list
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
void
pg_putfree (struct mm_struct *mm, int addr, struct pcb_t *caller)
{
  int pgn = PAGING_PGN (addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage (mm, pgn, &fpn, caller) != 0)
    return; /* invalid page access */
#ifdef MMDBG
  printf("\tFree fpn: %d\n", fpn);
#endif

  MEMPHY_put_freefp (caller->mram, fpn);
}
/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int
__free (struct pcb_t *caller, int vmaid, int rgid)
{
  struct vm_rg_struct rgnode;
  int size;

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1;

  /* Manage the collect freed region to freerg_list */
  rgnode = caller->mm->symrgtbl[rgid];
  size = rgnode.rg_end - rgnode.rg_start;

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list (caller->mm, rgnode);

#ifdef MMDBG
  print_list_rg (caller->mm->mmap->vm_freerg_list);
#endif

  pthread_mutex_lock (caller->mlock);
  /* enlist the obsolete memory frames*/
  for (int it = 0; it < size; it+=PAGING_PAGESZ)
    {
      pg_putfree (caller->mm, rgnode.rg_start + it, caller);
    }

  pthread_mutex_unlock (caller->mlock);

  return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int
pgalloc (struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{

#ifdef MMDBG
  printf ("\talloc PID=%d size=%d region=%d\n", proc->pid, size, reg_index);
#endif
  uint32_t addr;

  /* By default using vmaid = 0 */
  return __alloc (proc, 0, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int
pgfree_data (struct pcb_t *proc, uint32_t reg_index)
{
#ifdef MMDBG
  printf ("\tfree PID=%d region=%d\n", proc->pid, reg_index);
#endif
  return __free (proc, 0, reg_index);
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@data: data
 *@caller: pcb
 */
int
pg_getval (struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN (addr);
  int off = PAGING_OFFST (addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage (mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  return MEMPHY_read (caller->mram, phyaddr, data);
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int
pg_setval (struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN (addr);
  int off = PAGING_OFFST (addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage (mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  return MEMPHY_write (caller->mram, phyaddr, value);
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int
__read (struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid (caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num (caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pthread_mutex_lock (caller->mlock);

  pg_getval (caller->mm, currg->rg_start + offset, data, caller);

  pthread_mutex_unlock (caller->mlock);

  return 0;
}

/*pgread - PAGING-based read a region memory */
int
pgread (struct pcb_t *proc, // Process executing the instruction
        uint32_t source,    // Index of source register
        uint32_t offset,    // Source address = [source] + [offset]
        uint32_t destination)
{
  BYTE data;
  int val = __read (proc, 0, source, offset, &data);

  destination = (uint32_t)data;
#ifdef IODUMP
  printf ("\tread PID=%d region=%d offset=%d value=%d\n", proc->pid, source,
          offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl (proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump (proc->mram);
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int
__write (struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid (caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num (caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pthread_mutex_lock (caller->mlock);

  pg_setval (caller->mm, currg->rg_start + offset, value, caller);

  pthread_mutex_unlock (caller->mlock);

  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int
pgwrite (struct pcb_t *proc,   // Process executing the instruction
         BYTE data,            // Data to be wrttien into memory
         uint32_t destination, // Index of destination register
         uint32_t offset)
{
#ifdef IODUMP
  printf ("\twrite PID=%d region=%d offset=%d value=%d\n", proc->pid,
          destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl (proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump (proc->mram);
#endif

  return __write (proc, 0, destination, offset, data);
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int
free_pcb_memph (struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
    {
      pte = caller->mm->pgd[pagenum];

      if (!PAGING_PAGE_PRESENT (pte))
        {
          fpn = PAGING_FPN (pte);
          MEMPHY_put_freefp (caller->mram, fpn);
        }
      else
        {
          fpn = PAGING_SWP (pte);
          MEMPHY_put_freefp (caller->active_mswp, fpn);
        }
    }

  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: aligned size of the new region
 *@amount: amount of pages
 */
struct vm_rg_struct *
get_vm_area_node_at_brk (struct pcb_t *caller, int vmaid, int size, int amount)
{
  struct vm_rg_struct *newrg;
  struct vm_area_struct *cur_vma = get_vma_by_num (caller->mm, vmaid);

  newrg = malloc (sizeof (struct vm_rg_struct));

  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = cur_vma->sbrk + size;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int
validate_overlap_vm_area (struct pcb_t *caller, int vmaid, int vmastart,
                          int vmaend)
{
  /* TODO validate the planned memory area is not overlapped */
  /* Since we only have one region in this assignment,
   * We don't need this method
   */

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *@newrg: return new region
 */
int
inc_vma_limit (struct pcb_t *caller, int vmaid, int inc_sz,
               struct vm_rg_struct *newrg)
{
  int inc_amt = PAGING_PAGE_ALIGNSZ (
      inc_sz); /* increment amount: align to match pages size */
  int incnumpage = inc_amt / PAGING_PAGESZ;
  struct vm_area_struct *cur_vma = get_vma_by_num (caller->mm, vmaid);

  int old_end = cur_vma->vm_end;

  /*Validate overlap of obtained region: disabled */
  // if (validate_overlap_vm_area (caller, vmaid, area->rg_start, area->rg_end)
  //     < 0)
  // return -1; /*Overlap and failed allocation */

  /* The obtained vm area (only)
   * now will be alloc real ram region */
  cur_vma->vm_end += inc_amt;

  pthread_mutex_lock (
      caller->mlock); /* Locking physical memory access function */

  int map_ram_stat = vm_map_ram (caller, old_end, incnumpage, newrg);

#ifdef MMDBG
  print_list_rg (caller->mm->mmap->vm_freerg_list);
#endif

  pthread_mutex_unlock (caller->mlock);

  if (map_ram_stat < 0)
    return -1;             /* Map the memory to MEMRAM */

  cur_vma->sbrk += inc_sz; /* Increase the breaking point */

  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int
find_victim_page (struct mm_struct *mm, int *retpgn)
{
  struct pgn_t **pg = &mm->fifo_pgn;

  /* Implement the FIFO mechanism to find the victim page */
  if (*pg == NULL)
    { /* No page has been allocated */
      return -1;
    }

  while ((*pg)->pg_next != NULL)
    { /* Move to the last node */
      pg = &((*pg)->pg_next);
    }

  struct pgn_t *free_node = *pg; /* Temporarily node */

  *pg = NULL;                    /* Resetting the previous last node to NULL */

  free (free_node);              /* Free the last node */

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int
get_free_vmrg_area (struct pcb_t *caller, int vmaid, int size,
                    struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num (caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
    {
      if (rgit->rg_start + size <= rgit->rg_end)
        { /* Current region has enough space */
          newrg->rg_start = rgit->rg_start;
          newrg->rg_end = rgit->rg_start + size;

          /* Update left space in chosen region */
          if (rgit->rg_start + size < rgit->rg_end)
            {
              rgit->rg_start = rgit->rg_start + size;
            }
          else
            { /*Use up all space, remove current node */
              /*Clone next rg node */
              struct vm_rg_struct *nextrg = rgit->rg_next;

              /*Cloning */
              if (nextrg != NULL)
                {
                  rgit->rg_start = nextrg->rg_start;
                  rgit->rg_end = nextrg->rg_end;

                  rgit->rg_next = nextrg->rg_next;

                  free (nextrg);
                }
              else
                {                                /*End of free list */
                  rgit->rg_start = rgit->rg_end; // dummy, size 0 region
                  rgit->rg_next = NULL;
                }
            }
          break;
        }
      else
        {
          rgit = rgit->rg_next; // Traverse next rg
        }
    }

  if (newrg->rg_start == -1) // new region not found
    return -1;

  return 0;
}

// #endif
