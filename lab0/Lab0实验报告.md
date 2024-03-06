

# Lab0实验报告

### 1 实验过程

#### 1.1 搭载实验环境

虚拟机：Ubuntu 22.04.2 (Parallel Desktop)

安装编译内核所需要的交叉编译工具链和用于构建程序的软件包

![](/Users/liuyang/Desktop/lab0/截屏2023-09-19 16.40.20.png)

![](/Users/liuyang/Desktop/lab0/截屏2023-09-19 16.40.35.png)

安装qemu和gdb

![](/Users/liuyang/Desktop/lab0/截屏2023-09-19 16.41.11.png)

![](/Users/liuyang/Desktop/lab0/截屏2023-09-19 16.41.44.png)



#### 1.2 获取 Linux 源码和已经编译好的文件系统

linux版本：6.5.3

![](/Users/liuyang/Desktop/lab0/截屏2023-09-19 16.42.39.png)



#### 1.3 编译linux内核

```
$ make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- -j16
```

指令解释：

- `ARCH=riscv`: 编译risc-v平台内核
-  `CROSS_COMPILE=riscv64-linux-gnu-`: 指定使用的交叉编译工具链
- `-j16`: 16线程

![](/Users/liuyang/Desktop/lab0/截屏2023-09-19 16.48.28.png)



#### 1.4 配置menuconfig

![](/Users/liuyang/Desktop/lab0/截屏2023-09-19 17.08.06.png)



#### 1.5 使用qemu运行内核

```
$ qemu-system-riscv64 \
-nographic \
-machine virt \
-kernel linux-6.5.3/arch/riscv/boot/Image \
-device virtio-blk-device,drive=hd0 \
-append "root=/dev/vda ro console=ttyS0" \
-bios default \
-drive file=os23fall-stu/src/lab0/rootfs.img,format=raw,id=hd0
```

指令解释：

- `-nographic`: 不使用图形窗口，使用命令行
- `-machine`: 指定要 emulate 的机器
- `-kernel`: 指定内核 image
- `-append cmdline`: 使用 cmdline 作为内核的命令行
- `-device`: 指定要模拟的设备
- `-drive, file=<file_name>`: 使用 `file_name` 作为文件系统
- `-S`: 启动时暂停 CPU 执行
- `-s`: `-gdb tcp::1234`的简写
- `-bios default`: 使用默认的 OpenSBI firmware 作为 bootloader

![](/Users/liuyang/Desktop/lab0/截屏2023-09-19 20.21.22.png)



#### 1.6 使用GDB对内核进行调试

```
# Terminal 1
$ qemu-system-riscv64 \
-nographic \
-machine virt \
-kernel linux-6.5.3/arch/riscv/boot/Image \
-device virtio-blk-device,drive=hd0 \
-append "root=/dev/vda ro console=ttyS0" \
-bios default \
-drive file=os23fall-stu/src/lab0/rootfs.img,format=raw,id=hd0 -S -s

# Terminal 2
$ gdb-multiarch linux-6.5.3/vmlinux
(gdb) target remote :1234   # 连接 qemu
(gdb) b start_kernel        # 设置断点
(gdb) continue              # 继续执行
(gdb) quit                  # 退出 gdb
```

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 16.43.31.png)

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 20.20.21.png)

### 2 思考题

#### 2.1 使用 `riscv64-linux-gnu-gcc` 编译单个 `.c` 文件

`riscv64-linux-gnu-gcc -c -o main.o main.c`

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 16.25.50.png)

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 16.26.04.png)

#### 2.2 使用 `riscv64-linux-gnu-objdump` 反汇编 1 中得到的编译产物

`riscv64-linux-gnu-objdump main.o > main.o.txt`

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 16.33.41.png)



#### 2.3 调试 Linux 

##### a.在 GDB 中查看汇编代码

`layout asm`

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 16.45.09.png)

##### b.在 0x80000000 处下断点

`b * 0x80000000`

##### c.查看所有已下的断点

`info breakpoints`

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 16.50.23.png)

##### d.在 0x80200000 处下断点

`b * 0x80200000`

##### e.清除 0x80000000 处的断点

`delete 1`

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 16.57.20.png)

##### f.继续运行直到触发 0x80200000 处的断点

`c`

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 16.58.20.png)

##### g.单步调试一次

`next`会报错：Cannot find bounds of current function，可能原因是该函数实际上没有被成功调用

`si`:执行单条指令

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 20.26.00.png)

##### h.退出QEMU

在gdb中`quit`,然后`ctrl + A`松开后按`X`退出QEMU。



#### 2.4 使用 `make` 工具清除 Linux 的构建产物

`make mrproper`删除所有编译产物和配置文件

![](/Users/liuyang/Desktop/lab0/截屏2023-09-21 20.31.20.png)



#### 2.5 `vmlinux` 和 `Image` 的关系和区别是什么？

关系：

`vmlinux`是elf格式的原始内核文件，而`Image`是Linux编译内核时使用`objcopy`处理`vmlinux`后生成的二进制内核映像，即`vmlinux`经过`objcopy`处理后可以得到`Image`。

区别：
`vmlinux`是elf格式文件，不能直接引导linux启动。`Image`是二进制文件，可以直接引导linux启动。

