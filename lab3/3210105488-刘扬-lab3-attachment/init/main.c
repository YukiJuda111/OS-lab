#include "printk.h"
#include "sbi.h"
#include "defs.h"

extern void test();

int start_kernel() {
    printk("2022");
    printk(" Hello RISC-V\n");
    printk("sstatus: ");
    printk("%d\n", csr_read(sstatus));

    csr_write(sscratch,5488);
    printk("sscratch: ");
    printk("%d\n", csr_read(sscratch));

    test(); // DO NOT DELETE !!!
    
	return 0;
}
