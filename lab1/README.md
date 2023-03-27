#### 姓名：王道宇

#### 学号：PB21030794

### 一、裁剪Linux内核

- **思考题**
  
  - `sudo apt update`是更新当前的所有已安装的扩展包
  
  - `sudo apt install build-essential libncurses-dev bison flex libssl-dev libelf-dev qemu-system-x86`是安装`QEMU`的命令，相当于编译Linux所需的依赖。

- 对`Linux`内核修改的过程中,可以有多个选项可供配置:
  
  - 如果在开始生成默认配置：`make defconfig` ，不经修改，大概编译后生成的`bzImage` 有$12M$左右
  
  - 如果在开始生成空配置：`make allnoconfig`，只配置必要选项，参考选项：[2018osh课程主页](https://osh-2018.github.io/2/kernel/)，保存修改退出后，最终生成的`bzImage`约有$1.7M$

- 【**可选**】测试哪些选项真正能影响编译后`bzImage`文件的大小：
  
  - 我们先生成默认配置，已知编译后产生的`bzImage`文件约$12M$
  
  - 去除`Mitigations for speculative execution vulnerabilities`（推测执行漏洞的缓解措施），编译后生成的`bzImage`文件约$11.1M$
  
  - 再去除`Virtualization `（虚拟化），编译后产生的`bzImage`文件仍然约$11.1M$
  
  - 再去除`Enable loadable module support`（启用可加载模块支持），编译后产生的`bzImage`文件为$10.8M$
  
  - 再去除`Networking support` （网络支持），编译后产生的`bzImage`文件为$7.5M$
  
  - 再去除所有`Device Drivers`（设备驱动）、`File systems`（文件系统）、`Security options`（安全选项）内部所有的选项，编译产生的`bzImage`文件为$4.4M$
  
  - 再去除`Cryptographic API`（加密API），编译产生的`bzImage`文件为$4.3M$
  
  - 再去除`kernal hacking`（内核监视）、`Library routine`（库例程）和`Memory Management options`（内存管理选项），编译产生的`bzImage`文件为$3.6M$

- 可以看出：`Networking support` （网络支持）、`Device Drivers`（设备驱动）、`File systems`（文件系统）、`Security options`（安全选项）对编译产生的`bzImage`文件的大小影响最大。

- 最终`bzImage`的大小如下：
  
  ![](https://github.com/Melmaphother/osh-2023-labs/blob/main/lab1/pic/%E6%9C%80%E7%BB%88bzImage.png)

### 二、创建初始内存盘

- 按照提示进行操作，在屏幕中输出结果为：
  
  ![](https://github.com/Melmaphother/osh-2023-labs/blob/main/lab1/pic/%E6%9C%80%E7%BB%88%E8%BE%93%E5%87%BA%E5%AD%A6%E5%8F%B7.png)

- 【**可选**】
  
  如果删去最后一行：
  
  `while(1){};`
  
  会导致初始内存盘直接跳出进程导致创建失败，后续进程找不到父进程，最终导致**kernel panic**.

### 三、添加自定义系统调用

- 经过前两步操作，我们只需要编写`initrd`程序测试这个自定义的`syscall`就可以，查阅资料可得，需要调取的头文件为：
  
  ```cpp
  #define _GNU_SOURCE
  #define TRANSFER 548
  #include <signal.h>
  #include <sys/syscall.h>
  #include <sys/types.h>
  #include <unistd.h>
  ```

- `syscall`的调用方式为：
  
  ```cpp
  ret1 = syscall(TRANSFER, buf1, len1);
  ```

- 最终`qemu`的输出结果为
  
  ![](https://github.com/Melmaphother/osh-2023-labs/blob/main/lab1/pic/%E6%9C%80%E7%BB%88%E8%BE%93%E5%87%BA%E5%AD%97%E7%AC%A6%E4%B8%B2.png)
