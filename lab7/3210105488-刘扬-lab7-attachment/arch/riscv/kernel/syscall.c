#include "syscall.h"
#include "printk.h"
#include "proc.h"

extern struct task_struct* current;

int64_t sys_write(unsigned int fd, const char* buf, uint64_t count) {
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        ret = target_file->write(target_file, (const void*)buf, count);
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}

int64_t sys_read(unsigned int fd, char* buf, uint64_t count) {
    int64_t ret;
    struct file* target_file = &(current->files[fd]);
    if (target_file->opened) {
        ret = target_file->read(target_file, (const void*)buf, count);
    } else {
        printk("file not open\n");
        ret = ERROR_FILE_NOT_OPEN;
    }
    return ret;
}

void syscall(struct pt_regs* regs) {
    if (regs->x[17] == SYS_WRITE) {
        regs->x[10] = sys_write((unsigned int)regs->x[10], (const char*)regs->x[11], regs->x[12]);
    } 
    else if (regs->x[17] == SYS_GETPID) {
        regs->x[10] = current->pid;
    } 
    else if (regs->x[17] == SYS_READ){
        regs->x[10] = sys_read((unsigned int)regs->x[10], (char*)regs->x[11], regs->x[12]);
    }
    else {
        printk("Unhandled Syscall: 0x%lx\n", regs->x[17]);
        while (1);
    }
    regs->sepc += 4;
}