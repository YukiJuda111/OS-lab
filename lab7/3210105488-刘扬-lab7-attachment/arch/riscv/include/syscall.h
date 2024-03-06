#ifndef _SYSCALL_H
#define _SYSCALL_H
#define SYS_WRITE   64
#define SYS_GETPID  172
#define SYS_READ    63
#include "types.h"
struct pt_regs {
    uint64 x[32];
    uint64 sepc;
    uint64 sstatus;
};
int64_t sys_write(unsigned int fd, const char* buf, uint64_t count);
int64_t sys_read(unsigned int fd, char* buf, uint64_t count);
void syscall(struct pt_regs*);
#endif