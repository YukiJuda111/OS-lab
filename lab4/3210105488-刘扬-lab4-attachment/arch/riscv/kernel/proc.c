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

    // 1. 调用 kalloc() 为 idle 分配一个物理页
    idle = (struct task_struct*)kalloc();
    // 2. 设置 state 为 TASK_RUNNING;
    idle->state = TASK_RUNNING;
    // 3. 由于 idle 不参与调度 可以将其 counter / priority 设置为 0
    idle->counter = 0;
    idle->priority = 0;
    // 4. 设置 idle 的 pid 为 0
    idle->pid = 0;
    // 5. 将 current 和 task[0] 指向 idle
    current = idle;
    task[0] = idle;
    // 1. 参考 idle 的设置, 为 task[1] ~ task[NR_TASKS - 1] 进行初始化
    // 2. 其中每个线程的 state 为 TASK_RUNNING, 此外，为了单元测试的需要，counter 和 priority 进行如下赋值：
    //      task[i].counter  = task_test_counter[i];
    //      task[i].priority = task_test_priority[i];
    // 3. 为 task[1] ~ task[NR_TASKS - 1] 设置 `thread_struct` 中的 `ra` 和 `sp`,
    // 4. 其中 `ra` 设置为 __dummy （见 4.3.2）的地址,  `sp` 设置为 该线程申请的物理页的高地址

    for (int i = 0;i < NR_TASKS;i++ ){
        task[i] = (struct task_struct*)kalloc();
        task[i]->state = TASK_RUNNING;
        task[i]->counter = task_test_counter[i];
        task[i]->priority = task_test_priority[i];
        task[i]->pid = i;

        task[i]->thread.ra = (uint64)__dummy;
        task[i]->thread.sp = (uint64)(task[i]) + PGSIZE;

        //task[i]->thread.sepc = USER_START;
        // unsigned long sstatus = csr_read(sstatus);
        // sstatus &= ~(1 << 8); // sstatus[SPP] = 0
        // sstatus |= 1 << 5; // sstatus[SPIE] = 1
        // sstatus |= 1 << 18; // sstatus[SUM] = 1
        // task[i]->thread.sstatus = sstatus;
        // task[i]->thread.sscratch = USER_END;

        task[i]->thread_info.kernel_sp = (unsigned long)(task[i])+PGSIZE;
        // task[i]->thread_info.user_sp = kalloc();

        pagetable_t pgtbl = (pagetable_t)kalloc();
        for(int j = 0;j < PGSIZE / sizeof(uint64*);j++){
            pgtbl[j] = swapper_pg_dir[j];
        }

        // uint64 num_pages = ((uint64)_eramdisk - (uint64)_sramdisk) / PGSIZE;
        // if (num_pages * PGSIZE < ((uint64)_eramdisk - (uint64)_sramdisk)){
        //     num_pages++;
        // }
        // char *_uapp = (char *)alloc_pages(num_pages);
        // for(int j = 0;j < ((uint64)_eramdisk - (uint64)_sramdisk);j++){
        //     _uapp[j] = _sramdisk[j];
        // }
        // unsigned long va = USER_START;
        // unsigned long pa = (unsigned long)(_uapp) - PA2VA_OFFSET;
        // create_mapping(pgtbl, va, pa, num_pages * PGSIZE, 31);
        // va = USER_END - PGSIZE;
        // pa = task[i]->thread_info.user_sp - PA2VA_OFFSET;
        // create_mapping(pgtbl, va, pa, PGSIZE, 23);

        load_program(task[i], pgtbl);
        uint64 satp = (uint64)0x8 << 60; 
        satp |= ((uint64)(pgtbl) - PA2VA_OFFSET) >> 12;
        task[i]->satp = satp;
    }
    printk("...proc_init done!\n");

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
    for (int i = 1; i < phdr_cnt; i++) {
        phdr = (Elf64_Phdr*)(phdr_start + sizeof(Elf64_Phdr) * i);
        if (phdr->p_type == PT_LOAD) {
            // alloc space and copy content
            uint64 num_pages = (phdr->p_memsz) / PGSIZE;
            if (num_pages * PGSIZE < phdr->p_memsz){
                num_pages++;
            }
            char *start_addr = (char *)(_sramdisk + phdr->p_offset);
            char *alloc_mem = (char *)alloc_pages(num_pages);
            for(int j = 0;j < phdr->p_memsz;j++){
                alloc_mem[j] = start_addr[j];
            }
            // clear
            memset((char *)((uint64)alloc_mem + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz); 
            // allign
            uint64 va = phdr->p_vaddr;
            uint64 offset = va % PGSIZE;
            for(int i = num_pages * PGSIZE - offset - 1;i >= 0;i--){
                alloc_mem[i + offset] = alloc_mem[i];
            }
            memset(alloc_mem, 0, offset);
            va = va - offset;
            uint64 pa = (uint64)alloc_mem - PA2VA_OFFSET;
            create_mapping(pgtbl, va, pa, num_pages * PGSIZE, 31);
        }
    }
    // set user stack
    task->thread_info.user_sp = kalloc();
    // pc for the user program
    task->thread.sepc = ehdr->e_entry;
    // sstatus bits set
    unsigned long sstatus = csr_read(sstatus);
    sstatus &= ~(1 << 8); // sstatus[SPP] = 0
    sstatus |= 1 << 5; // sstatus[SPIE] = 1
    sstatus |= 1 << 18; // sstatus[SUM] = 1
    task->thread.sstatus = sstatus;

    task->thread.sscratch = USER_END;

    // user stack for user program
    uint64 va = USER_END - PGSIZE;
    uint64 pa = task->thread_info.user_sp - PA2VA_OFFSET;
    create_mapping(pgtbl, va, pa, PGSIZE, 23);
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
        while(++i < NR_TASKS){
            if(task[i]->state == TASK_RUNNING && task[i]->counter == 0){
                zero_cnt++;
                continue;
            }
            if(task[i]->state == TASK_RUNNING && (int)task[i]->counter < c){
                c = task[i]->counter;
                next = task[i];
            }
        }
        if(zero_cnt != NR_TASKS - 1) break;
        i = NR_TASKS;
        while(--i){
            task[i]->counter = rand();
            printk("SET [PID = %d PRIORITY = %d COUNTER = %d]\n",task[i]->pid, task[i]->priority, task[i]->counter);
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



