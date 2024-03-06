#include "defs.h"
#include "types.h"
#include "vm.h"
#include "mm.h"
#include "string.h"
#include "printk.h"

/* early_pgtbl: 用于 setup_vm 进行 1GB 的 映射。 */
uint64  early_pgtbl[512] __attribute__((__aligned__(0x1000)));

void setup_vm(void) {
    /* 
    1. 由于是进行 1GB 的映射 这里不需要使用多级页表 
    2. 将 va 的 64bit 作为如下划分： | high bit | 9 bit | 30 bit |
        high bit 可以忽略
        中间9 bit 作为 early_pgtbl 的 index
        低 30 bit 作为 页内偏移 这里注意到 30 = 9 + 9 + 12， 即我们只使用根页表， 根页表的每个 entry 都对应 1GB 的区域。 
    3. Page Table Entry 的权限 V | R | W | X 位设置为 1
    */

    memset(early_pgtbl, 0x0, PGSIZE);
    unsigned long PA = PHY_START;
    unsigned long VA1 = PA;
    unsigned long VA2 = PA + PA2VA_OFFSET;
    int index;
    index = (VA1 >> 30) & 0x1ff;
    early_pgtbl[index] = ((PA >> 12) << 10) | 0xf;
    index = (VA2 >> 30) & 0x1ff;
    early_pgtbl[index] = ((PA >> 12) << 10) | 0xf;
}

/* swapper_pg_dir: kernel pagetable 根目录， 在 setup_vm_final 进行映射。 */
uint64  swapper_pg_dir[512] __attribute__((__aligned__(0x1000)));
extern char _stext[];
extern char _srodata[];
extern char _sdata[];

void setup_vm_final(void) {
    memset(swapper_pg_dir, 0x0, PGSIZE);
    // No OpenSBI mapping required

    // mapping kernel text X|-|R|V
    create_mapping(swapper_pg_dir, (uint64)_stext, (uint64)_stext - PA2VA_OFFSET, _srodata - _stext, 11);
    // mapping kernel rodata -|-|R|V
    create_mapping(swapper_pg_dir, (uint64)_srodata, (uint64)_srodata - PA2VA_OFFSET, _sdata - _srodata, 3);
    // mapping other memory -|W|R|V
    create_mapping(swapper_pg_dir, (uint64)_sdata, (uint64)_sdata - PA2VA_OFFSET, PHY_SIZE - (_sdata - _stext), 7);

    // set satp with swapper_pg_dir
    uint64 set_satp = (((uint64)(swapper_pg_dir) - PA2VA_OFFSET) >> 12) | ((uint64)8 << 60);
    csr_write(satp, set_satp);

    // flush TLB
    asm volatile("sfence.vma zero, zero");

    // flush icache
    asm volatile("fence.i");

    printk("...setup_vm_final done!\n");
    return;
}


/**** 创建多级页表映射关系 *****/
/* 不要修改该接口的参数和返回值 */
create_mapping(uint64 *pgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm) {
    uint64 cur_vpn; 
    uint64* cur_pgtbl; 
    uint64 cur_pte; 
    uint64* new_pg;

    // int i = sz / PGSIZE + 1; 
    while (sz > 0)
    {
        cur_pgtbl = pgtbl;
        cur_vpn = ((uint64)(va) >> 30) & 0x1ff;
        cur_pte = *(cur_pgtbl + cur_vpn);
        if (!(cur_pte & 1)) {
            new_pg = (uint64*)kalloc();
            cur_pte = ((((uint64)new_pg - PA2VA_OFFSET) >> 12) << 10) | 0x1;
            *(cur_pgtbl + cur_vpn) = cur_pte;
        }

        cur_pgtbl = (uint64*)(((cur_pte >> 10) << 12) + PA2VA_OFFSET);
        cur_vpn = ((uint64)(va) >> 21) & 0x1ff;
        cur_pte = *(cur_pgtbl + cur_vpn);
        if (!(cur_pte & 1)) {
            new_pg = (uint64*)kalloc();
            cur_pte = ((((uint64)new_pg - PA2VA_OFFSET) >> 12) << 10) | 0x1;
            *(cur_pgtbl + cur_vpn) = cur_pte;
        }

        cur_pgtbl = (uint64*)(((cur_pte >> 10) << 12) + PA2VA_OFFSET);
        cur_vpn = ((uint64)(va) >> 12) & 0x1ff;
        cur_pte = ((pa >> 12) << 10) | (perm & 0x1ff);
        *(cur_pgtbl + cur_vpn) = cur_pte;

        va += PGSIZE;
        pa += PGSIZE;
        sz -= PGSIZE;
    }
    return;
}