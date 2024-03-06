.extern start_kernel

    .section .text.init
    .globl _start
_start:
    # set sp first for the call
    la sp,boot_stack_top # sp <- address of stack

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
    csrsi sstatus, 1<<1

    la sp,boot_stack_top # sp <- address of stack
    call start_kernel # jump to start_kernel

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096 # stack size

    .globl boot_stack_top
boot_stack_top:
