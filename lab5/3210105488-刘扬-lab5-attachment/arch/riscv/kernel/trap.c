#include "printk.h"
#include "clock.h"
#include "proc.h"
#include "syscall.h"
#include "vm.h"
#include "defs.h"
extern struct task_struct* current;
extern char _sramdisk[];

void trap_handler(unsigned long scause, unsigned long sepc, unsigned long stval, struct pt_regs *regs) {
    if (scause == 0x8000000000000005) {
        clock_set_next_event();
        do_timer();
        return;
   }  
   else if (scause == 8) {
        syscall(regs);
        return;
    } 
    else if(scause == (uint64)0xc || scause == (uint64)0xd || scause == (uint64)0xf) { 
        printk("[S] Supervisor Page Fault, scause: %lx, stval: %lx, sepc: %lx\n", scause, stval, sepc);
        do_page_fault(regs, stval);
    }
    else {
        printk("[S] Unhandled Trap, scause: %lx, stval: %lx, sepc: %lx\n", scause, stval, sepc);
        while(1);
    }
}

void do_page_fault(struct pt_regs *regs, unsigned long stval) {
    struct vm_area_struct *vma = find_vma(current, stval);   
    char *new_page = alloc_page();
    create_mapping(current->pgd, PGROUNDDOWN(stval), (uint64)new_page-PA2VA_OFFSET, PGSIZE, (vma->vm_flags | 0x11));
    if(!(vma->vm_flags & VM_ANONYM)){ 
        char *src_addr = (char*)(_sramdisk + vma->vm_content_offset_in_file);
        if(stval - PGROUNDDOWN(vma->vm_start) < PGSIZE){
            uint64 offset = stval % PGSIZE;
            for(int j = 0;j < PGSIZE - offset;j++){
                new_page[j + offset] = src_addr[j];
            }
        }
        else{
            uint64 pg_offset = (stval - PGROUNDDOWN(vma->vm_start)) / PGSIZE;
            uint64 offset = stval % PGSIZE;
            for(int j = 0;j < PGSIZE;j++){
                new_page[j] = src_addr[j + PGSIZE * pg_offset - offset];
            }
        }
    }  
}