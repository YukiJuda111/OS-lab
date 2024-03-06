#ifndef _SYSCALL_H
#define _SYSCALL_H
#define SYS_WRITE   64
#define SYS_GETPID  172
#define SYS_CLONE   220
#include "types.h"
#include "defs.h"
struct pt_regs {
    uint64 x[32];
    uint64 sepc;
    uint64 sstatus;
    uint64 stval;
    uint64 sscratch;
    uint64 scause;
};
void syscall(struct pt_regs*);
int find_empty_task();
uint64 sys_clone(struct pt_regs*);
void copy_vma();
int walk_pgd(uint64*, uint64, uint64*);
#endif