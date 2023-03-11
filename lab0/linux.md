### linux环境下的设备信息
- 查看内核/操作系统   uname -a
- 查看操作系统版本    head -n 1 /etc/issue
- 查看CPU信息         cat /prok/cpuinfo
- 查看计算机名        hostname
- 列出所有的PCI设备   lspci -tv
- 列出所有的USB设备   lsusb -tv
- 查看所有环境变量    env
- 查看所有进程        ps -ef
- 查看内存总量        grep MemTotal /prok/meminfo
- 查看剩余内存总量    grep MemFree  /prok/meminfo
- 查看所有磁盘分区    fdisk -l

### 设备截图
![截图1](https://github.com/Melmaphother/OSH-2023-labs/blob/main/lab0/src/%E8%AE%BE%E5%A4%87%E4%BF%A1%E6%81%AF1.png)
![截图2](https://github.com/Melmaphother/OSH-2023-labs/blob/main/lab0/src/%E8%AE%BE%E5%A4%87%E4%BF%A1%E6%81%AF2.png)
![截图3](https://github.com/Melmaphother/OSH-2023-labs/blob/main/lab0/src/%E8%AE%BE%E5%A4%87%E4%BF%A1%E6%81%AF3.png)