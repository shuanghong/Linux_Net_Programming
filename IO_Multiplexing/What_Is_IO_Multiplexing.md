# 什么是 IO 复用

IO多路复用(multiplexing)的本质是通过一种机制(内核缓冲I/O数据), 让单个进程/线程可以监视多个文件描述符. 一旦某个描述符就绪(可读或可写), 通知应用程序进行相应的读写操作.   
比如某个socket可读的时候, 内核告知用户, 用户执行read操作, 这样就可以保证每次read都能读到有效数据, 再配合非阻塞socket使用, 单个进程就可以同时监视多个socket的读事件, 多个socket的IO操作都能在一个线程内交替地顺序完成, 这就叫IO多路复用, 这里的复用指的是复用同一个线程.  

socket()函数创建的套接字默认都是阻塞的, 当线程执行socket相关的API时, 调用不能立即完成, 线程一直处于等待状态直到操作完成获得结果或者超时. 如果线程阻塞于某一个socket的读写时, 将无法处理其它socket的请求, 这无疑是十分低效的. 当然可以采用多线程, 但大量的线程占用很大的内存空间, 并且线程切换会带来很大的开销. 而IO多路复用模型的优点就是单个线程能支持更多的并发连接请求.  
会引起阻塞的socket API分为以下四种:

* 外出连接: connect(). 对于TCP连接, 客户端以阻塞套接字为参数, 调用该函数向服务器发起连接. 该函数在收到服务器的应答前, 不会返回.这意味着TCP连接总会等待至少服务器的一次往返时间.
* 接受连接: accept(). 服务端以阻塞套接字为参数调用该函数, 等待客户端的连接请求, 如果此时没有连接请求. 线程就会进入睡眠状态.
* 读操作: read()、recv()、recvfrom(). 以阻塞套接字为参数调用该函数接收数据时, 如果套接字缓冲区内没有数据可读, 则调用线程在数据到来前一直睡眠.
* 写操作: write()、send()、sendto(). 以阻塞套接字为参数调用该函数发送数据时, 如果套接字缓冲区没有可用空间, 线程会一直睡眠直到有空间可写.

参考:  
[https://zhuanlan.zhihu.com/p/22834126](https://zhuanlan.zhihu.com/p/22834126)  
[https://www.zhihu.com/question/28594409](https://www.zhihu.com/question/28594409)  
[https://www.zhihu.com/question/32163005](https://www.zhihu.com/question/32163005)

## IO 模型
一次IO操作, 以read为例, 分为两个阶段:

* 阶段一, 等待数据就绪,到达内核缓冲区(Waiting for the data to be ready).
* 阶段二, 将数据从内核空间拷贝到用户空间(Copying the data from the kernel to the process)

### 阻塞式IO(blocking IO)
如上所述, 默认情况所有的socket都是阻塞的, 以数据包套接字 recvfrom() 为例, 过程如下:

![](http://i.imgur.com/6oRQvoY.gif) 

应用程序调用 recvfrom(), 如果没有数据收到则线程或者进程就会被挂起, 直到数据到达并且被复制到应用进程的缓冲区或者发生错误才返回(最常见的错误是被信号中断).  
进程在IO操作的两个阶段中都处于阻塞状态.

### 非阻塞IO(non-blocking IO)
将IO设为非阻塞, 当执行read时如果没有数据, 则返回错误, 如EWOULDBLOCK. 这样就不会阻塞线程, 但是需要不断的轮询来读取或写入. 过程如下:

![](http://i.imgur.com/WqfQjTs.gif)

对于网络IO来说, IO的第一阶段通常比第二阶段花更多的时间, 所以非阻塞式IO旨在将第一阶段非阻塞化. 调用非阻塞式read时, 如果数据未就绪则直接返回; 反之则阻塞式地进行第二阶段的IO.  
在数据未就绪的情况下通常需要结合轮询机制, 保证数据就绪的IO能够被及时的处理, 这对CPU的消耗也是比较大的.

### IO 复用(也称为 event driven IO)
对于IO复用来说, 进程/线程通常阻塞在select、poll、epoll上, 而不是阻塞在真正的IO系统调用read/write上, 其好处是可以同时监听多个IO的状态. 基本原理是用户进程调用select/epoll不断的轮询所负责的IO, 当某个IO有数据到达时, 由内核通知用户进程. 过程如下:

![](http://i.imgur.com/PwqL85w.gif)

IO复用本身也是阻塞的, 以select为例, 在调用select期间用户进程也被阻塞, 直到内核通知数据就绪才返回, 并且在IO操作的阶段二同样阻塞. 

### 信号驱动(signal driven) IO

使用信号驱动IO, 要求内核当文件描述符准备就绪以后发送相应的信号通知我们. 与IO复用有类似之处, 但它是通过内核的信号机制实现非阻塞的就绪通知. 过程如下:

![](http://i.imgur.com/K0qRis8.gif)

### 异步 IO

异步IO, 由POSIX规范定义, 其工作机制是: 告知内核启动某个操作, 并让内核在整个操作完成后通知我们. 就是说, 对于IO操作的那两个阶段, 用户进程都不用关心, 当两个阶段都完成后由内核通知进程. 过程如下:

![](http://i.imgur.com/OpetmX2.gif)

进程调用aio_read()给内核传递描述符、缓冲区指针、缓冲区大小和文件偏移, 并告诉内核整个操作完成时如何通知我们. 该系统调用立即返回, 而且在等待I/O完成期间, IO两个阶段都是非阻塞的.  
对比信号驱动IO, 主要区别在于: 信号驱动由内核告诉我们何时可以开始一个IO操作(数据在内核缓冲区中), 而异步IO则由内核通知IO操作何时已经完成(数据已经在用户空间中).  
为了实现异步IO, POSIX专门定义了一套以aio开头的API, 如: aio_read.

### 各种 IO 模型的比较

![](http://i.imgur.com/0oqlJ3e.gif)

![](http://i.imgur.com/nabD2jp.png)

前四种模型的区别在于IO的第一阶段, 第二阶段都是一样的, 阻塞和非阻塞区别在于调用blocking IO会一直block住对应的进程直到操作完成, 而non-blocking IO在kernel还准备数据的情况下会立刻返回.  
前四种模型都是同步式IO, POSIX 关于同步、异步的定义是:
	
	A synchronous I/O operation causes the requesting process to be blocked until that I/O operation completes;
    An asynchronous I/O operation does not cause the requesting process to be blocked;
即:

	同步I/O操作: 导致请求进程阻塞，直到I/O操作完成;
	异步I/O操作: 不导致请求进程阻塞.

两者的区别就在于synchronous IO做”IO operation”的时候会将进程阻塞, 按照这个定义，之前所述的blocking IO、non-blocking IO、IO multiplexing 都属于synchronous IO.   
定义中所指的”IO operation”是指真实的IO操作, non-blocking IO 在执行recvfrom时的IO阶段二进程是被block的. 而asynchronous IO当进程发起IO操作之后, 就直接返回再也不理睬了, 直到内核告知进程说IO完成, 在这整个过程中, 进程完全没有被block.

## 关于同步、异步, 阻塞、非阻塞
参考:

[https://www.zhihu.com/question/19732473](https://www.zhihu.com/question/19732473)

[http://www.cnblogs.com/Anker/p/5965654.html](http://www.cnblogs.com/Anker/p/5965654.html)

[http://blog.csdn.net/historyasamirror/article/details/5778378](http://blog.csdn.net/historyasamirror/article/details/5778378)









