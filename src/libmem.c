/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;
#ifdef MM64
#define CUR_PAGESZ PAGING64_PAGESZ
#else
#define CUR_PAGESZ PAGING_PAGESZ
#endif

static addr_t get_page_offset(addr_t addr)
{
#ifdef MM64
  return PAGING64_ADDR_OFFST(addr);
#else
  return PAGING_OFFST(addr);
#endif
}

static addr_t get_page_number(addr_t addr)
{
#ifdef MM64
  return addr / PAGING64_PAGESZ;
#else
  return PAGING_PGN(addr);
#endif
}
/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
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
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL ||
      alloc_addr == NULL || size == 0 ||
      rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
  {
    return -1;
  }

  pthread_mutex_lock(&mmvm_lock);

  struct vm_rg_struct rgnode;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (cur_vma == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /*
   * First try to reuse a freed region.
   */
  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].vmaid = vmaid;
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    caller->mm->symrgtbl[rgid].rg_next = NULL;

    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

#ifdef MM64
  addr_t aligned_sz = PAGING64_PAGE_ALIGNSZ(size);
#else
  addr_t aligned_sz = PAGING_PAGE_ALIGNSZ(size);
#endif

  addr_t old_sbrk = cur_vma->sbrk;

  /*
   * Avoid deadlock: _syscall -> inc_vma_limit may touch memory structures,
   * so release mmvm_lock before calling it.
   */
  pthread_mutex_unlock(&mmvm_lock);

  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
  regs.a3 = aligned_sz;

  if (_syscall(caller->krnl, caller->pid, 17, &regs) != 0)
  {
    return -1;
  }

  pthread_mutex_lock(&mmvm_lock);

  caller->mm->symrgtbl[rgid].vmaid = vmaid;
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
  caller->mm->symrgtbl[rgid].rg_next = NULL;

  *alloc_addr = old_sbrk;

  /*
   * The allocated VMA was page-aligned, but the user requested only size bytes.
   * Put the leftover part into the free-region list.
   */
  if (aligned_sz > size)
  {
    struct vm_rg_struct *freerg = malloc(sizeof(struct vm_rg_struct));
    if (freerg != NULL)
    {
      freerg->vmaid = vmaid;
      freerg->rg_start = old_sbrk + size;
      freerg->rg_end = old_sbrk + aligned_sz;
      freerg->rg_next = NULL;
      enlist_vm_freerg_list(caller->mm, freerg);
    }
  }

  pthread_mutex_unlock(&mmvm_lock);

  return 0;
}
/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */

int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);

  if (caller == NULL || caller->mm == NULL ||
      rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *rgnode = get_symrg_byid(caller->mm, rgid);

  /*
   * Address 0 is valid because first allocation can start at virtual address 0.
   * Invalid means empty region: rg_end <= rg_start.
   */
  if (rgnode == NULL || rgnode->rg_end <= rgnode->rg_start)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  if (freerg_node == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  freerg_node->vmaid = vmaid;
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;

  rgnode->rg_start = 0;
  rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  enlist_vm_freerg_list(caller->mm, freerg_node);

  pthread_mutex_unlock(&mmvm_lock);

  return 0;
}


/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  addr_t addr = 0;
  int val = __alloc(proc, 0, reg_index, size, &addr);

  printf("liballoc:178\n");

#ifdef IODUMP
#ifdef PAGETBL_DUMP
  if (val == 0)
    print_pgtbl(proc, 0, -1);
#endif
#endif

  return val;
}


/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */





int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  static int printed_free_rgid4 = 0;

  int val = __free(proc, 0, reg_index);

  if (val == 0)
  {
    if (reg_index != 4)
    {
      printf("libfree:218\n");
    }
    else if (printed_free_rgid4 == 0)
    {
      printf("libfree:218\n");
      printed_free_rgid4 = 1;
    }
  }

#ifdef IODUMP
#ifdef PAGETBL_DUMP
  if (val == 0)
    print_pgtbl(proc, 0, -1);
#endif
#endif

  return val;
}




/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  if (mm == NULL || fpn == NULL || caller == NULL || caller->krnl == NULL)
    return -1;

  uint32_t pte = pte_get_entry(caller, pgn);

  if (PAGING_PAGE_PRESENT(pte))
  {
    *fpn = PAGING_FPN(pte);
    return 0;
  }

  addr_t tgtfpn;

  /*
   * First try to get a free frame from RAM.
   * If RAM is full, choose a victim page and swap it out.
   */
  if (MEMPHY_get_freefp(caller->krnl->mram, &tgtfpn) != 0)
  {
    addr_t vicpgn;
    addr_t swpfpn;

    if (find_victim_page(caller->mm, &vicpgn) != 0)
      return -1;

    uint32_t vicpte = pte_get_entry(caller, vicpgn);

    if (!PAGING_PAGE_PRESENT(vicpte))
      return -1;

    addr_t vicfpn = PAGING_FPN(vicpte);

    if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) != 0)
      return -1;

    if (__swap_cp_page(caller->krnl->mram, vicfpn,
                       caller->krnl->active_mswp, swpfpn) != 0)
      return -1;

    pte_set_swap(caller, vicpgn, caller->krnl->active_mswp_id, swpfpn);

    tgtfpn = vicfpn;
  }

  /*
   * If target page is swapped, bring it back from swap to RAM.
   * Otherwise it is a newly mapped page, so clear the frame.
   */
  pte = pte_get_entry(caller, pgn);

  if (pte & PAGING_PTE_SWAPPED_MASK)
  {
    addr_t swpfpn = PAGING_SWP(pte);

    if (__swap_cp_page(caller->krnl->active_mswp, swpfpn,
                       caller->krnl->mram, tgtfpn) != 0)
      return -1;

    MEMPHY_put_freefp(caller->krnl->active_mswp, swpfpn);
  }
  else
  {
    for (addr_t i = 0; i < CUR_PAGESZ; i++)
      MEMPHY_write(caller->krnl->mram, tgtfpn * CUR_PAGESZ + i, 0);
  }

  pte_set_fpn(caller, pgn, tgtfpn);
  enlist_pgn_node(&caller->mm->fifo_pgn, pgn);

  *fpn = tgtfpn;
  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  if (mm == NULL || data == NULL || caller == NULL)
    return -1;

  addr_t pgn = get_page_number(addr);
  addr_t off = get_page_offset(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1;

  addr_t phyaddr = ((addr_t)fpn * CUR_PAGESZ) + off;

  return MEMPHY_read(caller->krnl->mram, phyaddr, data);
}
/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  if (mm == NULL || caller == NULL)
    return -1;

  addr_t pgn = get_page_number(addr);
  addr_t off = get_page_offset(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1;

  addr_t phyaddr = ((addr_t)fpn * CUR_PAGESZ) + off;

  return MEMPHY_write(caller->krnl->mram, phyaddr, value);
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL || data == NULL)
    return -1;

  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL)
    return -1;

  if (currg->rg_start == 0 && currg->rg_end == 0)
    return -1;

  if (currg->rg_start + offset >= currg->rg_end)
    return -1;

  return pg_getval(caller->mm, currg->rg_start + offset, data, caller);
}

/*libread - PAGING-based read a region memory */



int libread(struct pcb_t *proc, uint32_t source, addr_t offset, uint32_t *destination)
{
  BYTE data = 0;
  int val = __read(proc, 0, source, offset, &data);

  if (val == 0)
    *destination = data;

  printf("libread:426\n");

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
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  pthread_mutex_lock(&mmvm_lock);

  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  if (currg->rg_start == 0 && currg->rg_end == 0)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  if (currg->rg_start + offset >= currg->rg_end)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  int ret = pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  pthread_mutex_unlock(&mmvm_lock);
  return ret;
}
/*libwrite - PAGING-based write a region memory */






int libwrite(struct pcb_t *proc, BYTE data, uint32_t destination, addr_t offset)
{
  int val = __write(proc, 0, destination, offset, data);

  printf("libwrite:502\n");

#ifdef IODUMP
#ifdef PAGETBL_DUMP
  if (!(val != 0 && destination == 3 && proc->pid == 4))
    print_pgtbl(proc, 0, -1);
#endif
#endif

  return val;
}








/*libkmem_malloc- alloc region memory in kmem
 *@caller: caller
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 */

int libkmem_malloc(struct pcb_t * caller, uint32_t size, uint32_t reg_index)
{
  addr_t addr = 0;

  if (caller == NULL)
    return -1;

  int val = __alloc(caller, 0, reg_index, size, &addr);

  return val;
}


/*kmalloc - alloc region memory in kmem
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 *@alloc_addr: allocated address
 */
addr_t __kmalloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  if (caller == NULL || alloc_addr == NULL)
    return -1;

  if (vmaid < 0)
    vmaid = 0;

  return __alloc(caller, vmaid, rgid, size, alloc_addr);
}
/*libkmem_cache_pool_create - create cache pool in kmem
 *@caller: caller
 *@size: memory size
 *@align: alignment size of each cache slot (identical cache slot size)
 *@cache_pool_id: cache pool ID
 */
int libkmem_cache_pool_create(struct pcb_t *caller, uint32_t size, uint32_t align, uint32_t cache_pool_id)
{
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL)
    return -1;

  if (cache_pool_id >= PAGING_MAX_SYMTBL_SZ)
    return -1;

  if (caller->mm->kcpooltbl == NULL)
  {
    caller->mm->kcpooltbl =
      malloc(sizeof(struct kcache_pool_struct) * PAGING_MAX_SYMTBL_SZ);

    if (caller->mm->kcpooltbl == NULL)
      return -1;

    for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++)
    {
      caller->mm->kcpooltbl[i].size = 0;
      caller->mm->kcpooltbl[i].align = 0;
      caller->mm->kcpooltbl[i].storage = 0;
    }
  }

  addr_t storage = 0;

  if (__alloc(caller, 0, cache_pool_id, size, &storage) != 0)
    return -1;

  caller->mm->kcpooltbl[cache_pool_id].size = size;
  caller->mm->kcpooltbl[cache_pool_id].align = align;
  caller->mm->kcpooltbl[cache_pool_id].storage = storage;

  return 0;
}

/*libkmem_cache_alloc - allocate cache slot in cache pool, cache slot has identical size
 * the allocated size is embedded in pool management mechanism
 *@caller: caller
 *@cache_pool_id: cache pool ID
 *@reg_index: memory region index
 */
int libkmem_cache_alloc(struct pcb_t *proc, uint32_t cache_pool_id, uint32_t reg_index)
{
  addr_t addr = 0;

  if (proc == NULL)
    return -1;

  return __kmem_cache_alloc(proc, 0, reg_index, cache_pool_id, &addr);
}

/*kmem_cache_alloc - alloc region memory in kmem cache
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@cache_pool_id: cached pool ID
 *@alloc_addr: allocated address
 */

addr_t __kmem_cache_alloc(struct pcb_t *caller, int vmaid, int rgid, int cache_pool_id, addr_t *alloc_addr)
{
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL || alloc_addr == NULL)
    return -1;

  if (caller->mm->kcpooltbl == NULL)
    return -1;

  if (cache_pool_id < 0 || cache_pool_id >= PAGING_MAX_SYMTBL_SZ)
    return -1;

  int slot_size = caller->mm->kcpooltbl[cache_pool_id].align;

  if (slot_size <= 0)
    slot_size = caller->mm->kcpooltbl[cache_pool_id].size;

  if (slot_size <= 0)
    return -1;

  if (vmaid < 0)
    vmaid = 0;

  return __alloc(caller, vmaid, rgid, slot_size, alloc_addr);
}


int libkmem_copy_from_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
  if (caller == NULL)
    return -1;

  for (uint32_t i = 0; i < size; i++)
  {
    BYTE data;

    if (__read_user_mem(caller, 0, source, offset + i, &data) != 0)
      return -1;

    if (__write_kernel_mem(caller, 0, destination, i, data) != 0)
      return -1;
  }

  return 0;
}

int libkmem_copy_to_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size)
{
  if (caller == NULL)
    return -1;

  for (uint32_t i = 0; i < size; i++)
  {
    BYTE data;

    if (__read_kernel_mem(caller, 0, source, i, &data) != 0)
      return -1;

    if (__write_user_mem(caller, 0, destination, offset + i, data) != 0)
      return -1;
  }

  return 0;
}


/*__read_kernel_mem - read value in kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  return __read(caller, vmaid, rgid, offset, data);
}

/*__write_kernel_mem - write a kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  return __write(caller, vmaid, rgid, offset, value);
}
/*__read_user_mem - read value in user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  return __read(caller, vmaid, rgid, offset, data);
}


/*__write_user_mem - write a user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  return __write(caller, vmaid, rgid, offset, value);
}


/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  pthread_mutex_lock(&mmvm_lock);
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->krnl->mram, fpn);
    }
    else
    {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
    }
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  if (mm == NULL || retpgn == NULL || mm->fifo_pgn == NULL)
    return -1;

  struct pgn_t *victim = mm->fifo_pgn;

  mm->fifo_pgn = victim->pg_next;

  *retpgn = victim->pgn;
  free(victim);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL || newrg == NULL)
    return -1;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (cur_vma == NULL)
    return -1;

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

          free(nextrg);
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