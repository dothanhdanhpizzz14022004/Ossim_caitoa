/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "os-mm.h"
#include <string.h>
#include "syscall.h"
#include "libmem.h"
#include "queue.h"
#include <stdlib.h>

#ifdef MM64
#include "mm64.h"
#else
#include "mm.h"
#endif

//typedef char BYTE;


int __sys_memmap(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
   int memop = regs->a1;
   BYTE value;

   /*
    * Kernel syscall context.
    * Do not allocate a fake kernel here.
    * Use the real kernel object passed from _syscall().
    */
   struct pcb_t caller;
   memset(&caller, 0, sizeof(struct pcb_t));
   caller.pid = pid;
   caller.krnl = krnl;

#ifdef MM_PAGING
   /*
    * Current baseline keeps the active memory context in krnl->mm.
    * Later, when we fully support per-process proc->mm, this can be
    * changed to lookup PCB by pid from krnl->running_list.
    */
   caller.mm = krnl->mm;
#endif

   switch (memop) {
   case SYSMEM_MAP_OP:
            vmap_pgd_memset(&caller, regs->a2, regs->a3);
            break;
   case SYSMEM_INC_OP:
            inc_vma_limit(&caller, regs->a2, regs->a3);
            break;
   case SYSMEM_SWP_OP:
            __mm_swap_page(&caller, regs->a2, regs->a3);
            break;
   case SYSMEM_IO_READ:
            MEMPHY_read(krnl->mram, regs->a2, &value);
            regs->a3 = value;
            break;
   case SYSMEM_IO_WRITE:
            MEMPHY_write(krnl->mram, regs->a2, regs->a3);
            break;
   default:
            printf("Memop code: %d\n", memop);
            break;
   }

   return 0;
}



