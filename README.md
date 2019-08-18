## FileShare

## Introduction：

本项目是一个使用C++11编写的局域网文件共享服务器，可用于局域网内的文件传输下载

|Part I| Part II |
|--|--|
| [服务端](https://github.com/Ran1366/FileShare/blob/master/服务端.md) |  [客户端](https://github.com/Ran1366/FileShare/blob/master/客户端.md)  |

## 运行过程

程序运行时，自己既是客户端也是服务端，作为服务端将自己想要的共享的文件放在指定的文件目录下，客户端可开启多线程快速匹配在线的服务端，得到所有的在线服务端地址，选择其中的一个查看，该服务端的共享文件进行下载，对于大文件执行多线程分块传输，小文件则直接传输
并输出到显示屏上

## Environment

OS：Linux version 3.10.0-862.el7.x86_64

EDITOR：NVIM v0.3.2-881-gd81b9d5ec

COMPILER: gcc version 5.3.1 20160406 (Red Hat 5.3.1-6) (GCC)


## Technical points

 - http协议，局域网内各个主机间进行通信都使用http协议
 - TCP/IP，使用TCP/IP协议栈对数据进行封装和分用
 - httplib，使用httplib库提供的接口建立http连接
 - Boost库，使用Boost库中的文件操作，线程库，进行文件的传输
