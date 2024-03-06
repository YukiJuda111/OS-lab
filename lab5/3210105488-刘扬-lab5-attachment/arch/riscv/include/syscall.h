#ifndef _SYSCALL_H
#define _SYSCALL_H
#define SYS_WRITE   64
#define SYS_GETPID  172
#include "types.h"
struct pt_regs {
    uint64 x[32];
    uint64 sepc;
    uint64 sstatus;
    uint64 stval;
    uint64 sscratch;
    uint64 scause;
};
void syscall(struct pt_regs*);
#endif