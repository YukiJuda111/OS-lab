.extern start_kernel

    .section .text.init
    .globl _start
_start:
    # set sp first for the call
    la sp,boot_stack_top # sp <- address of stack
    li t0, 0xffffffdf80000000
    sub sp, sp, t0
    
    call setup_vm
    call relocate
    # memory init
    call mm_init
    call setup_vm_final
    # task init
    call task_init

    # set stvec = _traps
    la t0, _traps
    csrw stvec, t0

    # set sie[STIE] = 1
    li t0, 1<<5
    csrs sie, t0

    # set first time interrupt
    rdtime a0
    li t0, 10000000
    add a0, a0, t0
    call sbi_set_timer

    # set sstatus[SIE] = 1
    #csrsi sstatus, 1<<1

    la sp,boot_stack_top # sp <- address of stack
    call start_kernel # jump to start_kernel

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)
    li t0, 0xffffffdf80000000
    add ra, ra, t0 
    add sp, sp, t0 

    # set satp with early_pgtbl
    li t0, 8
    slli t0, t0, 60
    la t1, early_pgtbl
    li t2, 0xffffffdf80000000
    sub t1, t1, t2
    srli t1, t1, 12
    or t0, t0, t1
    csrw satp, t0

    # flush tlb
    sfence.vma zero, zero

    # flush icache
    fence.i
    
    ret

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 # stack size

    .globl boot_stack_top
boot_stack_top:
