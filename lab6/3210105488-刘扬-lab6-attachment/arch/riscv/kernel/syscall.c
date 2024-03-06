#include "syscall.h"
#include "printk.h"
#include "proc.h"
#include "string.h"
#include "vm.h"
#include "mm.h"

extern struct task_struct* current;
extern void __ret_from_fork();
extern uint64  swapper_pg_dir[];
extern struct task_struct* task[NR_TASKS]; 

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
    else if (regs->x[17] == SYS_CLONE) {
        regs->x[10] = sys_clone(regs);
    }
}

int find_empty_task(){
    for(int i = 1;i < NR_TASKS; i++){
        if(task[i] == NULL){
            return i;
        }
    }
}

uint64 sys_clone(struct pt_regs *regs) {
    /*
     1. 参考 task_init 创建一个新的 task, 将的 parent task 的整个页复制到新创建的 
        task_struct 页上(这一步复制了哪些东西?）。将 thread.ra 设置为 
        __ret_from_fork, 并正确设置 thread.sp
        (仔细想想，这个应该设置成什么值?可以根据 child task 的返回路径来倒推)

     2. 利用参数 regs 来计算出 child task 的对应的 pt_regs 的地址，
        并将其中的 a0, sp, sepc 设置成正确的值(为什么还要设置 sp?)

     3. 为 child task 申请 user stack, 并将 parent task 的 user stack 
        数据复制到其中。 

     3.1. 同时将子 task 的 user stack 的地址保存在 thread_info->
        user_sp 中，如果你已经去掉了 thread_info，那么无需执行这一步

     4. 为 child task 分配一个根页表，并仿照 setup_vm_final 来创建内核空间的映射

     5. 根据 parent task 的页表和 vma 来分配并拷贝 child task 在用户态会用到的内存

     6. 返回子 task 的 pid
    */

    struct task_struct *child = (struct task_struct*)kalloc();
    for(int i = 0;i < PGSIZE;i++){
        ((char*)child)[i] = ((char*)current)[i];
    }
    child->thread.ra = (uint64)__ret_from_fork;

    int empty_idx = find_empty_task();
    child->pid = empty_idx;
    task[empty_idx] = child;

    uint64 offset = (uint64)regs % PGSIZE;
    struct pt_regs *child_regs = (struct pt_regs*)(child + offset);

    for(int i = 0;i < 37 * 8;i++){
        ((char*)child_regs)[i] = ((char*)regs)[i];
    }
    //__switch_to->__ret_from_fork(in _traps)->user program
    child->thread.sp = (uint64)child_regs; // for __switch_to
    child_regs->x[2] = (uint64)child_regs; // for _traps
    child_regs->x[10] = 0; //return 0 (child thread)
    child_regs->sepc = regs->sepc + 4;

    uint64 child_stack = alloc_page();
    for(int i = 0;i < PGSIZE;i++){
        ((char*)child_stack)[i] = ((char*)(USER_END - PGSIZE))[i];
    }
    child->thread_info.kernel_sp = (uint64)(child)+PGSIZE;
    child->thread_info.user_sp = (uint64)USER_END;

    child->pgd = (uint64*)alloc_page(); 
    uint64 satp = (uint64)0x8 << 60; 
    satp |= ((uint64)(child->pgd) - PA2VA_OFFSET) >> 12;
    child->satp = satp;
    for(int i = 0;i < PGSIZE;i++){
        ((char*)child->pgd)[i] = ((char*)swapper_pg_dir)[i];
    }
    create_mapping(child->pgd, USER_END-PGSIZE, (uint64)child_stack-PA2VA_OFFSET, PGSIZE, 23);

    copy_vma(child->pgd);

    printk("[S] New task: %d\n", empty_idx);
    return empty_idx;
}

void copy_vma(uint64* pgd){
    for (int i = 0; i < current->vma_cnt; i++) {
        struct vm_area_struct *cur_vma = &(current->vmas[i]);
        uint64 cur_addr = cur_vma->vm_start;
        while (cur_addr < cur_vma->vm_end) {
            walk_pgd(current->pgd, PGROUNDDOWN(cur_addr), pgd);
            cur_addr += PGSIZE;
        }
    }
}

int walk_pgd(uint64* parent_pgd, uint64 va, uint64* child_pgd){
    uint64 cur_vpn; 
    uint64* cur_pgtbl; 
    uint64 cur_pte; 
    cur_pgtbl = parent_pgd;
    cur_vpn = ((uint64)(va) >> 30) & 0x1ff;
    cur_pte = *(cur_pgtbl + cur_vpn);
    if (!(cur_pte & 1)) {
        return;
    }
    cur_pgtbl = (uint64*)(((cur_pte >> 10) << 12) + PA2VA_OFFSET);
    cur_vpn = ((uint64)(va) >> 21) & 0x1ff;
    cur_pte = *(cur_pgtbl + cur_vpn);
    if (!(cur_pte & 1)) {
        return;
    }
    cur_pgtbl = (uint64*)(((cur_pte >> 10) << 12) + PA2VA_OFFSET);
    cur_vpn = ((uint64)(va) >> 12) & 0x1ff;
    cur_pte = *(cur_pgtbl + cur_vpn);
    if (!(cur_pte & 1)) {
        return;
    }
    uint64 page = alloc_page();  
    create_mapping((uint64)child_pgd, va, (uint64)page-PA2VA_OFFSET, PGSIZE, 31);
    for(int i = 0;i < PGSIZE;i++){
        ((char*)page)[i] = ((char*)va)[i];
    }
    return;
}