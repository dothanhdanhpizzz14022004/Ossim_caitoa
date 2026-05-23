#include "common.h"
#include "syscall.h"
#include <stdio.h>

<<<<<<< HEAD
int __sys_hellohandler(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
    //printf("[SYSCALL] Hello world!\n");
    //printf("[SYSCALL] PID = %u\n", pid);
    //printf("[SYSCALL] Parameter a1 = %lu\n", (unsigned long)regs->a1);
    //printf("[SYSCALL] Parameter a2 = %lu\n", (unsigned long)regs->a2);

    return 0;
}
=======
int __sys_hellohandler(struct krnl_t *krnl, uint32_t pid, struct sc_regs* regs)
{
    printf("[SYSCALL] Hello world!\n");

    if (regs != NULL) {
        printf("[SYSCALL] Parameter a1 = %lu\n", regs->a1);
        printf("[SYSCALL] Parameter a2 = %lu\n", regs->a2);
        printf("[SYSCALL] Parameter a3 = %lu\n", regs->a3);
    }

    return 0;
}
>>>>>>> ea88219 (f)
