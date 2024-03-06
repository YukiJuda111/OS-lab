//arch/riscv/kernel/proc.c
#include "proc.h"
#include "mm.h"
#include "defs.h"
#include "rand.h"
#include "printk.h"
#include "test.h"
#include "vm.h"
#include "string.h"
#include "elf.h"

extern void __dummy();
extern uint64  swapper_pg_dir[];
struct task_struct* idle;           // idle process
struct task_struct* current;        // 指向当前运行线程的 `task_struct`
struct task_struct* task[NR_TASKS]; // 线程数组, 所有的线程都保存在此

/**
 * new content for unit test of 2023 OS lab2
*/
extern uint64 task_test_priority[]; // test_init 后, 用于初始化 task[i].priority 的数组
extern uint64 task_test_counter[];  // test_init 后, 用于初始化 task[i].counter  的数组

extern char _sramdisk[];
extern char _eramdisk[];


void task_init() {
    test_init(NR_TASKS);

    idle = (struct task_struct*)kalloc();
    idle->state = TASK_RUNNING;
    idle->counter = 0;
    idle->priority = 0;
    idle->pid = 0;
    current = idle;
    task[0] = idle;

    task[1] = (struct task_struct*)kalloc();
    task[1]->state = TASK_RUNNING;
    task[1]->counter = 0;
    task[1]->priority = 1;
    task[1]->pid = 1;

    task[1]->thread.ra = (uint64)__dummy;
    task[1]->thread.sp = (uint64)(task[1]) + PGSIZE;
    task[1]->thread_info.kernel_sp = (unsigned long)(task[1])+PGSIZE;


    pagetable_t pgtbl = (pagetable_t)kalloc();
    for(int j = 0;j < PGSIZE / sizeof(uint64*);j++){
        pgtbl[j] = swapper_pg_dir[j];
    }
    task[1]->pgd = pgtbl;

    load_program(task[1], pgtbl);
    uint64 satp = (uint64)0x8 << 60; 
    satp |= ((uint64)(pgtbl) - PA2VA_OFFSET) >> 12;
    task[1]->satp = satp;
    printk("[S] Initialized: pid: 1, priority: 1, counter: 0\n");
}

// arch/riscv/kernel/proc.c
void dummy() {
    schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if(current->counter == 1){
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1, leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
        }
    }
}

static uint64 load_program(struct task_struct* task, pagetable_t pgtbl) {
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)_sramdisk;

    uint64 phdr_start = (uint64)ehdr + ehdr->e_phoff;
    int phdr_cnt = ehdr->e_phnum;

    Elf64_Phdr* phdr;
    for (int i = 0; i < phdr_cnt; i++) {
        phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) {
            uint64 offset = (uint64)(phdr->p_vaddr) % PGSIZE;
            uint64 num_pages = (phdr->p_memsz + offset) / PGSIZE;
            if (num_pages * PGSIZE < (phdr->p_memsz + offset)){
                num_pages++;
            }
            uint64 length = num_pages * PGSIZE;
            do_mmap(task, phdr->p_vaddr, length, phdr->p_flags, phdr->p_offset, phdr->p_filesz);
        }
    }

    // 只进行vma的映射，并不进行读取，当真正访问该页时，才读取对应的页
    do_mmap(task, USER_END-PGSIZE, PGSIZE, VM_R_MASK | VM_W_MASK | VM_ANONYM, 0, 0); 
    task->thread.sepc = ehdr->e_entry;
    unsigned long sstatus = csr_read(sstatus);
    sstatus &= ~(1 << 8); // sstatus[SPP] = 0
    sstatus |= 1 << 5; // sstatus[SPIE] = 1
    sstatus |= 1 << 18; // sstatus[SUM] = 1
    task->thread.sstatus = sstatus;
    task->thread.sscratch = USER_END;
}

void do_timer(void) {
    // 1. 如果当前线程是 idle 线程 直接进行调度
    // 2. 如果当前线程不是 idle 对当前线程的运行剩余时间减1 若剩余时间仍然大于0 则直接返回 否则进行调度
    if (current == idle){
        schedule();
    } 
    else {
        if(current->counter){
            --(current->counter);
        }
        if ((int)current->counter > 0){ //current counter uint!!
            return;
        } 
        else {
            schedule();
        }
    }
}

extern void __switch_to(struct task_struct* prev, struct task_struct* next);

void switch_to(struct task_struct* next){
    if (current == next){
        return;
    }
    else{
        struct task_struct* prev = current;     
        current = next;
        printk("switch to [PID = %d PRIORITY = %d COUNTER = %d]\n", next->pid, next->priority, next->counter);
        __switch_to(prev, next);
    }
}


void schedule(void) {
#ifdef SJF

    struct task_struct* next;
    while(1){
        int c = __INT_MAX__;
        int i = 0;
        int zero_cnt = 0;
        int min_idx = -1;
        while(++i < NR_TASKS){
            if(task[i] != NULL && (int)task[i]->counter > 0 && (int)task[i]->counter < c){
                c = task[i]->counter;
                next = task[i];
                min_idx = i;
            }
        }
        if(min_idx != -1)break;
        i = NR_TASKS;
        while(--i){
            if(task[i] != NULL && task[i]->state == TASK_RUNNING){
                task[i]->counter = rand();
                printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n",task[i]->pid, task[i]->priority, task[i]->counter);               
            }
        }
    }
    switch_to(next);
#endif

#ifdef PRIORITY  
    struct task_struct* next;
    while(1){
        int c = -1;
        int i = NR_TASKS;
        while(--i){
            if(task[i]->state == TASK_RUNNING && (int)task[i]->counter > c){
                c = task[i]->counter;
                next = task[i];
            }            
        }
        if(c) break;
        i = NR_TASKS;
        while(--i){
            task[i]->counter = (task[i]->counter >> 1) + task[i]->priority;
            printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n", task[i]->pid, task[i]->priority, task[i]->counter);
        }
    }
    switch_to(next);
#endif
}

void do_mmap(struct task_struct *task, uint64 addr, uint64 length, uint64 flags,
    uint64 vm_content_offset_in_file, uint64 vm_content_size_in_file) {
    
    struct vm_area_struct* vma = &(task->vmas[task->vma_cnt]);
    task->vma_cnt++;
    vma->vm_start = addr;
    vma->vm_end = addr + length;
    vma->vm_flags = 0;
    if(flags & 1){
        vma->vm_flags += (1 << 3);
    }
    if(flags & 2){
        vma->vm_flags += (1 << 2);
    }
    if(flags & 4){
        vma->vm_flags += (1 << 1);
    }
    vma->vm_content_offset_in_file = vm_content_offset_in_file;
    vma->vm_content_size_in_file = vm_content_size_in_file;
}

struct vm_area_struct *find_vma(struct task_struct *task, uint64_t addr){
    for(int i = 0; i < task->vma_cnt; i++){
        if(addr >= task->vmas[i].vm_start && addr < task->vmas[i].vm_end){
            return &(task->vmas[i]);
        }
    }
    return NULL;
}
