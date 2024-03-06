#include "printk.h"
#include "sbi.h"
#include "defs.h"

extern void test();
extern void schedule();

extern char _stext[];
extern char _srodata[];
extern char _sdata[];

int start_kernel() {
    printk("2022");
    printk(" Hello RISC-V\n");

    schedule();
    test(); // DO NOT DELETE !!!
    
	return 0;
}
