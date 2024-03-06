#include "syscall.h"
#include "printk.h"
#include "proc.h"

extern struct task_struct* current;

void syscall(struct pt_regs* regs) {
    if (regs->x[17] == SYS_WRITE) {
        if (regs->x[10] == 1) {
            char* buf = (char*)regs->x[11];
            for (int i = 0; i < regs->x[12]; i++) {
                printk("%c", buf[i]);
            }
        regs->x[10] = regs->x[12];
        }
    } 
    else if (regs->x[17] == SYS_GETPID) {
        regs->x[10] = current->pid;
    } 
    regs->sepc += 4;
}