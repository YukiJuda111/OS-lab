# LAB2 实验报告

3210105488 刘扬

## 1 实验过程

本次实验需要实现的大概是：每次时钟中断触发时，当前线程的运行剩余时间-1，当当前线程运行剩余时间为0时，需要调用`schedule()`来找到下一个运行的线程，并通过`switch_to()`实现线程切换。

整体流程如下：

```
           Process 1         Operating System            Process 2
               +
               |                                            X
 P1 executing  |                                            X
               |                                            X
               v Timer Interrupt Trap                       X
               +---------------------->                     X
                                      +                     X
               X                  do_timer()                X
               X                      +                     X
               X                  schedule()                X
               X                      +                     X
               X              save state to PCB1            X
               X                      +                     X
               X           restore state from PCB2          X
               X                      +                     X
               X                      |                     X
               X                      v Timer Interrupt Ret
               X                      +--------------------->
               X                                            |
               X                                            |  P2 executing
               X                                            |
               X                       Timer Interrupt Trap v
               X                      <---------------------+
               X                      +
               X                  do_timer()
               X                      +
               X                  schedule()
               X                      +
               X              save state to PCB2
               X                      +
               X           restore state from PCB1
               X                      +
               X                      |
                 Timer Interrupt Ret  v
               <----------------------+
               |
 P1 executing  |
               |
               v
```

### 1.1 准备工作

将lab1的代码merge到lab2，修改`MakeFile`和`defs.h`，不赘述。

### 1.2 线程调度功能实现

#### 1.2.1 线程初始化

在初始化线程时，物理页的大小为4KB ，将`task_struct`存放在该页的低地址部分， 将线程的栈指针`sp`指向该页的高地址。具体内存布局如下图所示：

```
                ┌─────────────┐◄─── High Address
                │             │
                │    stack    │
                │             │
                │             │
          sp ──►├──────┬──────┤
                │      │      │
                │      ▼      │
                │             │
                │             │
                │             │
                │             │
4KB Page        │             │
                │             │
                │             │
                │             │
                ├─────────────┤
                │             │
                │             │
                │ task_struct │
                │             │
                │             │
                └─────────────┘◄─── Low Address
```

首先初始化`idle`即`task[0]`，给`idle`设置`task_struct`，并将`current`和`task[0]`都指向`idle`。

```c
  idle = (struct task_struct*)kalloc();
  idle->state = TASK_RUNNING;
  idle->counter = 0;
  idle->priority = 0;
  idle->pid = 0;
  current = idle;
  task[0] = idle;
```

然后要将`task[1]`~`task[NR_TASKS - 1]`全部初始化，需要设置好`ra`和`sp`。其中 `ra` 设置为 __dummy 的地址，`sp` 设置为 该线程申请的物理页的高地址。初始化`ra`的目的是让每个线程在第一次时钟中断时能够正确地进入`dummy()`

```c
  for (int i=0;i<NR_TASKS;i++ ){
      task[i] = (struct task_struct*)kalloc();
      task[i]->state = TASK_RUNNING;
      task[i]->counter = task_test_counter[i];
      task[i]->priority = task_test_priority[i];
      task[i]->pid = i;
      task[i]->thread.ra = (uint64)__dummy;
      task[i]->thread.sp = (uint64)(task[i]) + PGSIZE;
  }
```

#### 1.2.2 `__dummy`与`dummy`

当线程在运行时，由于时钟中断的触发，会将当前运行线程的上下文环境保存在栈上。当线程再次被调度时，会将上下文从栈上恢复，但是当我们创建一个新的线程，此时线程的栈为空，当这个线程被调度时，是没有上下文需要被恢复的，所以我们需要为线程 **第一次调度** 提供一个特殊的返回函数 `__dummy`

##### 1.2.2.1 dummy

`dummy`已经在实验指导中给出，在不触发时钟中断时，该线程会一直在`while`循环中执行，一旦触发中断，`current->counter`改变，`auto_inc_local_var`自增。

```c
void dummy() {
    schedule_test();
    uint64 MOD = 1000000007;
    uint64 auto_inc_local_var = 0;
    int last_counter = -1;
    while(1) {
        if ((last_counter == -1 || current->counter != last_counter) && current->counter > 0) {
            if(current->counter == 1){
                --(current->counter);   // forced the counter to be zero if this thread is going to be scheduled
            }                           // in case that the new counter is also 1，leading the information not printed.
            last_counter = current->counter;
            auto_inc_local_var = (auto_inc_local_var + 1) % MOD;
            printk("[PID = %d] is running. auto_inc_local_var = %d\n", current->pid, auto_inc_local_var);
        }
    }
}
```

##### 1.2.2.2 __dummy

为了线程在第一次调度时能够正确地进入`dummy`，简单来说，就是时钟中断在完成后的`sret`需要返回到`dummy`中。

```
    .globl __dummy
__dummy:
    la t0, dummy
    csrw sepc, t0
    sret
```

#### 1.2.3 实现线程切换

判断下一个执行的线程 `next` 与当前的线程 `current` 是否为同一个线程，如果是同一个线程，则无需做任何处理，否则调用 `__switch_to` 进行线程切换。

```c
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
```

在 `entry.S` 中实现线程上下文切换 `__switch_to`:

- `__switch_to`接受两个 `task_struct` 指针作为参数

- 保存当前线程的`ra`，`sp`，`s0~s11`到当前线程的 `thread_struct` 中

  ```
  sd ra, 48(a0) # Offset of thread_struct = 48
  sd sp, 56(a0)
  sd s0, 64(a0)
  ...
  sd s11, 152(a0)
  ```

- 将下一个线程的 `thread_struct` 中的相关数据载入到`ra`，`sp`，`s0~s11`中。

  ```
  ld ra, 48(a1) 
  ld sp, 56(a1)
  ld s0, 64(a1)
  ...
  ld s11, 152(a1)  
  ret
  ```

`thread_struct`的偏移量是根据`task_struct`的数据结构算出的:

在`thread_struct`前有6个`uint64`，6 * 8 = 48。

```c
struct thread_info {
    uint64 kernel_sp;
    uint64 user_sp;
};
struct task_struct {
    struct thread_info thread_info;
    uint64 state;    // 线程状态
    uint64 counter;  // 运行剩余时间
    uint64 priority; // 运行优先级 1最低 10最高
    uint64 pid;      // 线程id

    struct thread_struct thread;
};
```

#### 1.2.4 实现调度入口函数

实现 `do_timer()`，并在 `时钟中断处理函数` 中调用。

注意`current->counter`的类型是`uint64`，需要做类型转换。

```c
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
```

#### 1.2.5 实现线程调度

本次实验我们需要实现两种调度算法：1.短作业优先调度算法，2.优先级调度算法。

##### 1.2.5.1 短作业优先调度算法

当需要进行调度时按照一下规则进行调度：

- 遍历线程指针数组`task`（不包括 `idle` ，即 `task[0]` ）， 在所有运行状态 （`TASK_RUNNING`） 下的线程运行剩余时间`最少`的线程作为下一个执行的线程。
- 如果`所有`运行状态下的线程运行剩余时间都为0，则对 `task[1]` ~ `task[NR_TASKS-1]` 的运行剩余时间重新赋值 （使用 `rand()`） ，之后再重新进行调度。

参考[Linux v0.11 调度算法实现](https://elixir.bootlin.com/linux/0.11/source/kernel/sched.c#L122)。区别是Linux的调度算法是找到最大`counter`的线程，SJF是找到最小`counter`的线程。注意如果有多个线程的`counter`一样小，调度`pid`小的线程。

```c
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
```

##### 1.2.5.2 优先级调度算法

参考[Linux v0.11 调度算法实现](https://elixir.bootlin.com/linux/0.11/source/kernel/sched.c#L122)。

```c
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
```

### 1.3 编译及测试

![](/Users/liuyang/Desktop/OS/lab2/img/截屏2023-10-22 20.48.22.png)

![](/Users/liuyang/Desktop/OS/lab2/img/截屏2023-10-22 20.50.54.png)



## 2 思考题

##### 1.在 RV64 中一共用 32 个通用寄存器，为什么 `context_switch` 中只保存了14个?

每个线程都有自己的`ra`和`sp`，因此在做`context_switch`时需要保存上一个线程的`ra`和`sp`，并载入下一个线程的`ra`和`sp`。`s0`-`s11`是callee saved，在做`context_switch`时不会由caller进行保存，因此我们需要在callee中将`s0`-`s11`保存起来。

##### 2.当线程第一次调用时，其 `ra` 所代表的返回点是 `__dummy`。那么在之后的线程调用中 `context_switch` 中，`ra` 保存/恢复的函数返回点是什么呢? 请同学用 gdb 尝试追踪一次完整的线程切换流程，并关注每一次 `ra` 的变换 (需要截图)。

是否需要线程切换是由`do_timer`判断的，如果需要线程切换，即当前线程运行剩余时间为0时，会调用`schedule`来选择下一个线程，并在`schedule`中调用`switch_to`来切换线程。如果该线程是第一次调用，`ra`保存的是`__dummy`的地址。我们在`switch_to`打上断点，然后进行观察：

第一次线程切换时，由`task[0]`切换为`task[2]`：

![](/Users/liuyang/Desktop/OS/lab2/img/截屏2023-10-22 21.04.49.png)

进入`__switch_to`,查看到`ra`的值是`__dummy`的地址：

![](/Users/liuyang/Desktop/OS/lab2/img/截屏2023-10-22 21.06.41.png)

因为`ra`是`__dummy`的地址，所以在`ret`之后进入`__dummy`:

![](/Users/liuyang/Desktop/OS/lab2/img/截屏2023-10-22 21.08.08.png)

`__dummy`跳转到`dummy`，第一次线程切换完成，之后在`dummy`中每次触发时钟中断都会让`counter`-1。之后对于`task[1]`和`task[3]`的调度也同上，因为都是第一次调度，所以`ra`都是默认的`__dummy`的地址。

直接跳到第二次调度`task[2]`，进入`__switch_to`后观察`ra`的值：

![](/Users/liuyang/Desktop/OS/lab2/img/截屏2023-10-22 21.20.47.png)

可以看到`ra`变成了`0x80200804`，即`switch_to`函数中，调用`__switch_to`的下一行。因为在第一次调用时，我们`sd`了`ra`的值，即调用`__switch_to`后的返回地址在`thread_struct`中，因此我们`ld`得到的是`__switch_to`的返回地址。

![](/Users/liuyang/Desktop/OS/lab2/img/截屏2023-10-22 21.22.15.png)

这时候函数会逐层返回

```
__switch_to --> switch_to --> schedule --> do_timer --> trap_handler --> _traps
```

再由`_traps`中保存的`context`返回到正确的地址，即`sret`回到`dummy`中正在运行的行数。

在`_traps`的`sret`前打上断点即可验证：

![](/Users/liuyang/Desktop/OS/lab2/img/截屏2023-10-22 21.33.16.png)

`proc.c`的65行即：

![](/Users/liuyang/Desktop/OS/lab2/img/截屏2023-10-22 21.34.19.png)