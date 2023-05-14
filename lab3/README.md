## OSH Lab3 实验报告
### 文件简述
- 文件夹中包含源文件`src`和`src_epoll`，分别为线程池版本的服务器和`epoll`优化版服务器。
### 编译运行方法
- 在根目录提供了`makefile`，并且默认源是`src`文件夹，默认目标是建立`build`文件夹，在根目录终端输入`make`时，会在根目录下建立`build`文件夹，并将生成的可执行文件`server`放进`build`中，只需要输入:
  ```bash
  ./build/server
  ```
  即可运行线程池版本的服务器。
  若需要删除，直接输入`make clean`指令即可删除`build`文件夹。
- 当需要测试`epoll`优化版本的服务器时，请将`makefile`的三、四两行：
  ```txt
  BUILD_DIR := ./build
  SRC_DIRS := ./src
  ```
  修改为：
  ```txt
  BUILD_DIR := ./build_epoll
  SRC_DIRS := ./src_epoll
  ```
  在根目录终端输入`make`后，即会在根目录下建立`build_epoll`文件夹，，这时，只需要输入：
  ```bash
  ./build_epoll/server
  ```
  即可运行`epoll`优化版本的服务器。
  若需要删除，直接输入`make clean`指令即可删除`build_epoll`文件夹。
- 当然，为了比较性能优化的幅度，在根目录下也保留了一个`server.c`文件，这是一个没有使用任何优化的服务器，只需要在终端输入：
  ```bash
  gcc server.c -o server
  ```
  即可在根目录下生成一个可执行文件`server`，输入：
  ```bash
  ./server
  ```
  即可运行这个服务器。
### 实验目的
### 实验原理
### 实验结果