/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
    if (mm == NULL || mm->mmap == NULL)
    return NULL;

  struct vm_area_struct *pvma = mm->mmap;

  while (pvma != NULL)
  {
    if ((int)pvma->vm_id == vmaid)
      return pvma;

    pvma = pvma->vm_next;
  }

  return NULL;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn , addr_t swpfpn)
{
    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  struct vm_rg_struct * newrg;
  /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  
  if (cur_vma == NULL){
  printf ("Cannot find vm_area with id = %d\n", vmaid);
  return NULL;
  }
  

  newrg = (struct vm_rg_struct *)malloc(sizeof(struct vm_rg_struct));
  if (newrg == NULL){
  printf ("Malloc failed\n");
  return NULL;
  }

  /* TODO: update the newrg boundary*/
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;
  newrg->rg_next = NULL;

  printf("[MM-VM] New region in vma %d: [0x%lx - 0x%lx]\n",vmaid,(unsigned long)newrg->rg_start,(unsigned long)newrg->rg_end);

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
  //struct vm_area_struct *vma = caller->mm->mmap;
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL)
    return -1;

  /* TODO validate the planned memory area is not overlapped */
  if (vmastart >= vmaend)
  {
    return -1;
  }

  struct vm_area_struct *vma = caller->mm->mmap;
  if (vma == NULL)
  {
    return -1;
  }

  /* TODO validate the planned memory area is not overlapped */

  struct vm_area_struct *cur_area = get_vma_by_num(caller->mm, vmaid);
  if (cur_area == NULL)
  {
    return -1;
  }

  while (vma != NULL)
  {
    if (vma != cur_area && OVERLAP(vmastart, vmaend, vma->vm_start, vma->vm_end))
    {
      return -1;
    }
    vma = vma->vm_next;
  }
  /* End TODO*/

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz)
{
  if (caller == NULL || caller->mm == NULL || caller->krnl == NULL)
    return -1;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (cur_vma == NULL)
    return -1;

#ifdef MM64
  addr_t aligned_sz = PAGING64_PAGE_ALIGNSZ(inc_sz);
  addr_t pagesz = PAGING64_PAGESZ;
#else
  addr_t aligned_sz = PAGING_PAGE_ALIGNSZ(inc_sz);
  addr_t pagesz = PAGING_PAGESZ;
#endif

  int incpgnum = aligned_sz / pagesz;
  addr_t old_sbrk = cur_vma->sbrk;
  addr_t new_sbrk = old_sbrk + aligned_sz;

  if (validate_overlap_vm_area(caller, vmaid, old_sbrk, new_sbrk) < 0)
    return -1;

  for (int i = 0; i < incpgnum; i++)
  {
    addr_t vaddr = old_sbrk + i * pagesz;

#ifdef MM64
    addr_t pgn = vaddr / PAGING64_PAGESZ;
#else
    addr_t pgn = PAGING_PGN(vaddr);
#endif

    addr_t fpn;

    if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) != 0)
      return -1;

    pte_set_fpn(caller, pgn, fpn);
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }

  cur_vma->sbrk = new_sbrk;

  if (new_sbrk > cur_vma->vm_end)
    cur_vma->vm_end = new_sbrk;

  return 0;
}


// #endif
