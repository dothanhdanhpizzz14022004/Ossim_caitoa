/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "os-mm.h"
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
    if (krnl == NULL || regs == NULL)
      return -1;

   int memop = regs->a1;
   BYTE value;
   
   /* TODO THIS DUMMY CREATE EMPTY PROC TO AVOID COMPILER NOTIFY 
    *      need to be eliminated
	*/
struct pcb_t *caller = NULL;

   if (krnl->running_list != NULL)
   {
      for (int i = 0; i < krnl->running_list->size; i++)
      {
         if (krnl->running_list->proc[i] != NULL &&
             krnl->running_list->proc[i]->pid == pid)
         {
            caller = krnl->running_list->proc[i];
            break;
         }
      }
   }

   if (caller == NULL)
   {
      printf("Cannot find caller pid %u in running_list\n", pid);
      return -1;
   }

   /*
    * @bksysnet: Please note in the dual spacing design
    *            syscall implementations are in kernel space.
    */

   /* TODO: Traverse proclist to terminate the proc
    *       stcmp to check the process match proc_name
    */
//	struct queue_t *running_list = krnl->running_list;

    /* TODO Maching and marking the process */
    /* user process are not allowed to access directly pcb in kernel space of syscall */
    //....
	
   switch (memop) {
   case SYSMEM_MAP_OP:
            /* Reserved process case*/
			return vmap_pgd_memset(caller, regs->a2, regs->a3);

   case SYSMEM_INC_OP:
            return inc_vma_limit(caller, regs->a2, regs->a3);

   case SYSMEM_SWP_OP:
            return __mm_swap_page(caller, regs->a2, regs->a3);

   case SYSMEM_IO_READ:
            if (MEMPHY_read(caller->krnl->mram, regs->a2, &value) != 0)
               return -1;
            regs->a3 = value;
            return 0;

   case SYSMEM_IO_WRITE:
            return MEMPHY_write(caller->krnl->mram, regs->a2, regs->a3);

   default:
            printf("Memop code: %d\n", memop);
            return -1;
   }
}


