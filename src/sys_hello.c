#include "common.h"
#include "syscall.h"
#include "stdio.h"

int __sys_hellohandler(struct pcb_t *caller, struct sc_regs *regs)
{
    printf("[SYSCALL] Hello world!\n");
    printf("[SYSCALL] Parameter a1 = %lu\n", regs->a1);
    printf("[SYSCALL] Parameter a2 = %lu\n", regs->a2);
    
    return 0;   
}
