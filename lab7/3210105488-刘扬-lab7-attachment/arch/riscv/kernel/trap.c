#include "printk.h"
#include "clock.h"
#include "proc.h"
#include "syscall.h"

void trap_handler(unsigned long scause, unsigned long sepc, struct pt_regs *regs) {
    if (scause == 0x8000000000000005) {
        clock_set_next_event();
        do_timer();
        return;
   }  
   else if (scause == 8) {
        syscall(regs);
        return;
    } 
    else {
        printk("[S] Unhandled trap, ");
        printk("scause: %lx, ", scause);
        printk("stval: %lx\n", sepc);
        while (1);
    }
}