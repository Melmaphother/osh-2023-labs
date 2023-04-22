## 综述
- **姓名：王道宇**
- **学号：PB21030794**
- 可以在`lab2`目录下使用命令：
  ```txt
  make
  ```
  来生成可执行文件。
  `make`会在根目录`lab2`下生成一个`build`文件夹，文件夹下有`src`文件夹保存了所有`cpp`文件的中间文件，包括`.d`、`.o`文件，以及`shell`可执行文件，可以使用命令：
  ```txt
  ./build/shell
  ```
  来运行它。
  
- 文件框架
  ```bash
  .
  ├── build
  │   ├── final_program
  │   └── src
  │       ├── main.cpp.d
  │       ├── main.cpp.o
  │       ├── shell.cpp.d
  │       └── shell.cpp.o
  ├── lab2_pic
  │   ├── alias.png
  │   ├── background.png
  │   ├── cd_and_pwd.png
  │   ├── ctrl+d.png
  │   ├── echo.png
  │   ├── history.png
  │   ├── pipe.png
  │   ├── redir.png
  │   └── signal.png
  ├── Makefile
  ├── README.md
  ├── record_pic
  │   ├── morepipes.jpg
  │   └── pipe.jpg
  ├── Records.md
  └── src
      ├── main.cpp
      ├── shell.cpp
      └── shell.h

  5 directories, 22 files

  ```
- 代码框架：
  - main.cpp
  ```cpp
    main.cpp
    int main(){
      exe_ctrl_c;
      read_history_from_file;
      while(){
          exe_ctrl_d;
          cin << cmd;
          alias cmd;
          exe special cmd like !!;
          fork;
          exe_signal;
      }
    }
  ```
  - shell.cpp
  ```cpp
  shell.cpp
  ExePipe
     - ExeWithoutPipe
        -- ExeBuildinCmd
        -- ExeExternalCmd
           --- ExeCmdWithoutPipe
              ---- ExeSingleCmd
  ```
## 实验实现的功能
### 目录导航：
- **必做**
  - `cd`    10%
  - `pwd`    10%
- **选做**
  - 当`cd`命令没参数时，返回根目录    5%
- **结果展示**
![](README_pic/cd_and_pwd.png)
### 管道
- **实现两个管道**   10%
- **实现任意长度的管道**   10%
![](README_pic/pipe.png)
### 重定向
- **支持`<`重定向**    5%
- **支持`>`重定向**    5%
- **支持`>>`重定向**    5%
- **结果展示**
![](README_pic/redir.png)
- **经助教提醒，重定向需要支持类似`cmd < in > out`的指令**
  修改了重定向的实现，将从向量头开始找重定向符改成了从向量尾找
- **结果展示**
![](README_pic/redir_plus.png)
### 信号处理
- **支持ctrl+c丢弃当前命令**
- **ctrl+c正确终止当前运行的进程**
- **ctrl+c在shell嵌套的时候也可以正确终止当前运行的进程**
- **结果展示**
![](README_pic/signal.png)
### 前后台进程
- **后台程序运行**    10%
- **`wait`指令等待子进程结束**    10%
- **结果展示**
![](README_pic/background.png)
### 其余功能
**选做**
- **支持历史记录的命令**    5%
  - `history n`
  - `!!`
  - `!n`
  - **结果展示**
  ![](README_pic/history.png)
- **`alias`重命名命令**    5%
  - **结果展示**
  ![](README_pic/alias.png)
- **`echo ~`命令**    5%
- **`echo $`命令**    5%
  - **上面两个命令结果展示**
  ![](README_pic/echo.png)
- **`ctrl+d`命令**    5%
  - **结果展示**
  ![](README_pic/ctrl+d.png)
## 注释
- 选做一共做了**30%**
- 关于输入规范：
  - 允许输入命令时前后有空格（因为使用了trim函数去除了空格）
    例如允许输入
    ```bash
    $      cd     
    ```
    这样的命令。
  - 管道，重定向，外部命令的命令和参数之间请只使用**一个空格**
    例如
    ```bash
    $ ls    |    cat   -n
    ```
    是不被允许的。
    请写成
    ```bash
    $ ls | cat -n
    ```
  - 管道符，重定向符的两侧请输入一共空格。
    例如
    ```bash
    $ ls|cat -n
    $ cat>out
    ```
    是不被允许的，请写成
    ```bash
    $ ls | cat -n
    $ cat > out
    ```
  - 使用`alias`重命名时在`=`两边请不要加空格，类似这样：
    ```bash
    alias ll="ls -l"
    ```
（其实做输入检查也不是不行，但是这个貌似不是实验的重点。。。）