#include "common.h"
#include "syscall.h"
#include <stdio.h>

int __sys_hellohandler(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
    printf("[SYSCALL] Hello world!\n");
    printf("[SYSCALL] PID = %u\n", pid);
    printf("[SYSCALL] Parameter a1 = %lu\n", (unsigned long)regs->a1);
    printf("[SYSCALL] Parameter a2 = %lu\n", (unsigned long)regs->a2);

    return 0;
}
