# Lab1实验报告

3210105488 刘扬

### 1 实验过程

#### 1.1 内核引导

计算机上电到内核运行的过程如下：

```
   Hardware             RISC-V M Mode           RISC-V S Mode 
+------------+         +--------------+         +----------+
|  Power On  |  ---->  |  Bootloader  |  ---->  |  Kernel  |
+------------+         +--------------+         +----------+

```

Bootloader由OpenSbi完成，我们需要实现Kernel的引导和启动。

##### 1.1.1 编写head.SA sampling time of 125 μsec(1000000μs/8000) corresponds to 8000 samples per second. According to the Nyquist theorem, this is the sampling frequency needed to capture all the information in a 4 kHz channel, such as a telephone channel. 需要实现如下三点：

- 设置栈大小: 

  `.space 4096 # stack size`

- 设置栈指针:

   `la sp,boot_stack_top # sp <- address of stack`

- 跳转到start_kernel函数

  `call start_kernel # jump to start_kernel`

##### 1.1.2 完善 Makefile 脚本

补充`lib/Makefile`，使整个工程能够顺利构建。

上层在执行`make all`时需要`pirntk.o`,因此我们需要完成`printk.c`-->`printk.o`。

简而言之，只需要：

```makefile
all:printk.o
pirntk.o:printk.c
	${GCC}  ${CFLAG} -c printk.c
```

再将其写的规范一些，每行具体解释在注释中给出：

```makefile
C_SRC       = $(wildcard *.c) # C_SRC = printk.c
OBJ		    = $(patsubst %.c,%.o,$(C_SRC)) # patsubst: substitute %.c with %.o  OBJ = printk.o

all:$(OBJ) # target: printk.o

%.o:%.c # use .c to generate .o
	${GCC}  ${CFLAG} -c $< 
# &< : the first dependency, namely %.c or printk.c

clean:
	$(shell rm *.o 2>/dev/null)
```

##### 1.1.3 补充 `sbi.c`

补充sbi_ecall函数，利用内联汇编完成一下内容：

1. 将 ext (Extension ID) 放入寄存器 a7 中，fid (Function ID) 放入寄存器 a6 中，将 arg0 ~ arg5 放入寄存器 a0 ~ a5 中。
2. 使用 `ecall` 指令。`ecall` 之后系统会进入 M 模式，之后 OpenSBI 会完成相关操作。
3. OpenSBI 的返回结果会存放在寄存器 a0 ， a1 中，其中 a0 为 error code， a1 为返回值， 我们用 sbiret 来接受这两个返回值。

```c
struct sbiret sbi_ecall(int ext, int fid, uint64 arg0,
			            uint64 arg1, uint64 arg2,
			            uint64 arg3, uint64 arg4,
			            uint64 arg5) 
{
	struct sbiret ret;
    uint64 a6 = (uint64)(fid);
    uint64 a7 = (uint64)(ext);
	__asm__ volatile (
        "mv a7, %[ext]\n"
        "mv a6, %[fid]\n"
		"mv a0, %[arg0]\n"
		"mv a1, %[arg1]\n"
		"mv a2, %[arg2]\n"
		"mv a3, %[arg3]\n"
		"mv a4, %[arg4]\n"
		"mv a5, %[arg5]\n"
		"ecall\n"
		"mv %[error], a0\n"
		"mv %[ret_val], a1"
        : [error] "=r" (ret.error), [ret_val] "=r" (ret.value)
        : [ext] "r" (a7), [fid] "r" (a6), 
		  [arg0] "r" (arg0), [arg1] "r" (arg1), [arg2] "r" (arg2), [arg3] "r" (arg3), [arg4] "r" (arg4), [arg5] "r" (arg5) 
        : "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
    );
	return ret;
}
```

##### 1.1.4 修改 defs

```c
#define csr_read(csr)                       \
({                                          \
    register uint64 __v;                    \
    asm volatile ("csrr %0, " #csr          \
                    : "=r" (__v):           \
                    : "memory");            \
    __v;                                    \
})
```



#### 1.2 时钟中断

##### 1.2.1 准备工作

先修改 `vmlinux.lds`,`test.c` 以及 `head.S`

##### 1.2.2 开启trap处理

在运行 `start_kernel` 之前，我们要对上面提到的 CSR 进行初始化，初始化包括以下几个步骤：

1. 设置 `stvec`， 将`_traps` 所表示的地址写入 `stvec`，这里我们采用`Direct 模式` , 而 `_traps` 则是 trap 处理入口函数的基地址。

   ```
   la t0, _traps
   csrw stvec, t0
   ```

2. 开启时钟中断，将 `sie[STIE]` 置 1。`csrsi`最多支持五位立即数，因此不能直接使用`csrsi`。

   ```
   li t0, 1<<5
   csrs sie, t0
   ```

3. 设置第一次时钟中断。**因为这里用到了`call`，所以在这之前要先设置好`sp`，否则无法正确内核引导。在这里用到了自己写的`sbi_set_timer`函数，其实是调用了functionID = 0, ExtensionID = 0x00的`ecall`。**

   ```
   rdtime a0
   li t0, 10000000
   add a0, a0, t0
   call sbi_set_timer
   ```

4. 开启 S 态下的中断响应， 将 `sstatus[SIE]` 置 1。

   ```
   csrsi sstatus, 1<<1
   ```

##### 1.2.3 实现上下文切换

我们要使用汇编实现上下文切换机制， 包含以下几个步骤：

1. 在 `arch/riscv/kernel/` 目录下添加 `entry.S` 文件。

2. 保存 CPU 的寄存器（上下文）到内存中（栈上）。**需要先保存`sp`，因为在后续pop的过程中，`sp`需要最后恢复。同时还要保存`sepc`，保证后续`sret`能够返回到正确的地址。**

   ```
   sd sp, -8(sp)
   sd x0, -16(sp)
   sd x1, -24(sp)
   ...
   sd x31, -256(sp)
   csrr t0, sepc
   sd t0, -264(sp)
   addi sp, sp, -264
   ```

3. 将 `scause` 和 `sepc` 中的值传入 trap 处理函数`trap_handler`  ，我们将会在 `trap_handler` 中实现对 trap 的处理。

   ```
   csrr a0, scause
   csrr a1, sepc
   call trap_handler
   ```

4. 在完成对 trap 的处理之后， 我们从内存中（栈上）恢复 CPU 的寄存器（上下文）。**注意`sp`最后恢复。**

   ```
   ld t0, 0(sp)
   csrw sepc, t0
   ld x31, 8(sp)
   ...
   ld x0, 248(sp)
   ld sp, 256(sp)
   ```

5. 从 trap 中返回。

   ```
   sret
   ```

##### 1.2.4 实现 trap 处理函数

1. 在 `arch/riscv/kernel/` 目录下添加 `trap.c` 文件。

2. 在 `trap.c` 中实现 trap 处理函数`trap_handler()` , 其接收的两个参数分别是 `scause` 和 `sepc` 两个寄存器中的值。

   查表可知`scause`最高位是1，除最高位的其他位是5的时候是时钟中断。图片来源：[RISC-V特权级寄存器及指令文档_delegation register-CSDN博客](https://blog.csdn.net/Pandacooker/article/details/116423306)

   ![](/Users/liuyang/Desktop/OS/lab1/img/trap.png)

   ```c
   void trap_handler(unsigned long scause, unsigned long sepc) {
       if(scause == 0x8000000000000005){
           printk("[S] Supervisor Mode Timer Interrupt\n");
           clock_set_next_event();
       }
   }
   ```

##### 1.2.5 实现时钟中断相关函数

1. 在 `arch/riscv/kernel/` 目录下添加 `clock.c` 文件。

2. 在 `clock.c` 中实现 get_cycles ( ) : 使用 `rdtime` 汇编指令获得当前 `time` 寄存器中的值。

   ```c
   unsigned long get_cycles() {
       // 编写内联汇编，使用 rdtime 获取 time 寄存器中 (也就是mtime 寄存器 )的值并返回
       unsigned long time;
       __asm__ volatile (
           "rdtime %0"
           : "=r" (time)
       );
       return time;
   }
   ```

3. 在`clock.c`中实现 clock_set_next_event ( ) :调用`sbi_ecall`，设置下一个时钟中断事件。**同样用到了自己写的`sbi_set_timer`。**

   ```c
   void clock_set_next_event() {
       // 下一次 时钟中断 的时间点
       unsigned long next = get_cycles() + TIMECLOCK;
   
       // 使用 sbi_ecall 来完成对下一次时钟中断的设置
       sbi_set_timer(next);
   } 
   ```

##### 1.2.6 编译及测试

主要遇到了一个bug：在编译运行后，第一次产生时钟中断以后就陷入死循环。在用gdb调试以后发现每次在`entry.S`上下文切换时，`sret`的下一步又回跳回`entry.S`的第一行，且寄存器`sepc`的地址没有问题。

用gdb进入`trap_handler`函数后发现，该函数的汇编码只有`nop`，导致没有设置下一次的时钟中断，因此`mtime`的值永远是大于`mtimecmp`的，会一直发生时钟中断。

```c
void trap_handler(unsigned long scause, unsigned long sepc) {
    if (scause & 0x8000000000000000) {
        if (scause & 0x7FFFFFFFFFFFFFFF == 5)) {
            printk("[S] Supervisor Mode Timer Interrupt\n");
            clock_set_next_event();
        }
    }
}
```

错误原因可能是编译器自动优化了if的判断，认为if的条件永远不可能发生导致了错误，具体为什么我也不清楚。

![](/Users/liuyang/Desktop/OS/lab1/img/res.png)

### 2 思考题

#### 2.1 请总结一下 RISC-V 的 calling convention，并解释 Caller / Callee Saved Register 有什么区别？

RISC-V的calling convention定义了函数调用时寄存器的使用规则，包括参数传递、返回值、异常处理等。RISC-V的calling convention主要包括以下几个方面：

1. 参数传递：前8个整型参数通过a0-a7寄存器传递，浮点型参数通过f0-f7寄存器传递。
2. 返回值：整型返回值通过a0寄存器返回，浮点型返回值通过f0寄存器返回。
3. Caller Saved Register：在函数调用时，Caller Saved Register中的寄存器需要保存当前函数调用前的值，以便函数调用结束后恢复。Caller Saved Register包括t0-t6、a0-a7、f0-f7等寄存器。
4. Callee Saved Register：在函数调用时，Callee Saved Register中的寄存器需要保存当前函数调用中的值，以便函数调用结束后恢复。Callee Saved Register包括s0-s11、ra、gp、tp等寄存器。

Caller Saved Register和Callee Saved Register的区别在于，Caller Saved Register中的寄存器需要在函数调用前保存，因为函数调用后需要恢复原来的值；而Callee Saved Register中的寄存器需要在函数调用中保存，因为函数调用结束后需要恢复原来的值。

#### 2.2 编译之后，通过 System.map 查看 vmlinux.lds 中自定义符号的值（截图）。

`nm vmlinux`

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-04 16.33.49.png)

#### 2.3 用 `csr_read` 宏读取 `sstatus` 寄存器的值，对照 RISC-V 手册解释其含义（截图）。

#### 2.4 用 `csr_write` 宏向 `sscratch` 寄存器写入数据，并验证是否写入成功（截图）。

```c
printk("sstatus: ");
printk("%d\n", csr_read(sstatus));
csr_write(sscratch,5488);
printk("sscratch: ");
printk("%d\n", csr_read(sscratch));
```

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 19.58.36.png)

##### sstatus

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 20.06.12.png)

![](/Users/liuyang/Desktop/OS/lab1/img/11.png)

参考[RISC-V特权级寄存器及指令文档_delegation register-CSDN博客](https://blog.csdn.net/Pandacooker/article/details/116423306)

SPP表明进入S态之前的模式：当前值是0，为user mode

SIE是S mode中断的使能：当前值是1，允许s mode的中断发生

SPIE记录进入S态之前的中断使能情况：当前值是默认初始化的值0

UIE是user mode的中断使能

UPIE表明在处理u模式trap前，u模式中断是否使能

PUM(Protect User Memory) 修改s模式加载、存储和取指令访问虚拟内存的：当前值是0，翻译和保护行为正常

FS用于跟踪浮点扩展单元，两bit位的编码如下：`00`=off, `01`=initial, `10`=clean, `11`=dirty。当对应扩展的字段为off时，所有修改相应状态的指令都会产生非法指令异常；当字段为initial时，对应的状态都为初始化的值；当字段为Clean，所有状态可能与初始化值不同，当与上次保存的值相同；当前值是00，存在状态已经被修改

#### 2.5 Detail your steps about how to get `arch/arm64/kernel/sys.i`

将交叉编译工具链修改为arm64

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 21.57.25.png)

指定生成的文件:`sys.c` -> `sys.i`

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 21.55.58.png)

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 21.59.00.png)

#### 2.6 Find system call table of Linux v6.0 for `ARM32`, `RISC-V(32 bit)`, `RISC-V(64 bit)`, `x86(32 bit)`, `x86_64` List source code file, the whole system call table with macro expanded, screenshot every step.

##### 2.6.1 ARM32

system call table在`./arch/arm/include/unistd.h`

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 22.06.11.png)

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 22.05.59.png)

##### 2.6.2 RISCV 32/64

system call table在`./arch/riscv/include/asm/unistd.h`

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 22.08.15.png)

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 22.08.47.png)

##### 2.6.3 x86_32

system call table在`./arch/x86/entry/syscall/syscall_32.tbl`

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 22.23.54.png)

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 22.25.26.png)

##### 2.6.4 x86_64

system call table在`./arch/x86/entry/syscall/syscall_64.tbl`

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 22.26.22.png)

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 22.24.22.png)

#### 2.7 Explain what is ELF file? Try readelf and objdump command on an ELF file, give screenshot of the output. Run an ELF file and cat `/proc/PID/maps` to give its memory layout.

ELF（可执行和可链接格式）文件是Unix和类Unix操作系统中用于可执行文件、目标代码、共享库和核心转储的常见文件格式。它是一种二进制文件格式，包含有关程序的内存布局、符号表、重定位信息和其他执行或链接程序所需的详细信息。

操作系统的动态链接器使用ELF文件在运行时加载和链接共享库。它们还包含有关程序的入口点的信息，即程序运行时要执行的第一条指令。

##### 2.7.1 `readlf`和`objdump`

```
readlf -a main.o > readlf.txt
```

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 23.03.16.png)

```
objdump -d -S main.o > objdump.txt
```

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 23.07.18.png)

##### 2.7.2 Memory layout

先拿到pid

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 23.14.12.png)

在打印出memory layout

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 23.17.05.png)

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 23.18.17.png)

#### 2.8通过查看 `RISC-V Privileged Spec` 中的 `medeleg` 和 `mideleg` ，解释上面 `MIDELEG` 值的含义。

`medeleg`和`mideleg`是RISC-V架构中的两个控制寄存器，用于控制中断和异常的委托。其中，`medeleg`控制机器模式下的异常委托，`mideleg`控制中断委托。

`MIDELEG`值是`mideleg`寄存器的值，它指示了哪些中断可以被委托给超级用户模式（S模式）处理。具体来说，`MIDELEG`的每一位对应一个中断，如果该位被设置为1，则表示该中断可以被委托给S模式处理；如果该位被设置为0，则表示该中断不能被委托给S模式处理。

`MIDELEG`的二进制表示为`0b1000100010`，其中第1、5、9位为1，分别对应`Supervisor software interrupt`、`Supervisor timer interrupt`、`Supervisor external interrupt`。其他位为0，表示其他中断不能被委托给S模式处理。

![](/Users/liuyang/Desktop/OS/lab1/img/截屏2023-10-17 22.46.43.png)
