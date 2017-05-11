# IO 复用之 Select
select 函数的用途是在一段指定的时间内, 监听用户感兴趣的文件描述符上的可读、可写和异常等事件. 该函数允许进程指定内核等待多个事件中的任何一个发生, 并至少有一个事件发生或一段时间超时之后才唤醒. 例如调用 select, 告知内核仅在以下情况发生时返回:
	
- 文件描述符 1、4、5中任一描述符可读
- 文件描述符 2、3中任一描述符可写
- 文件描述符 6、7中有异常事件发生
- 超时 10.5s
也就是说, 调用 select 告知内核用户对哪些描述符的可读、可写、异常等事件感兴趣,以及等待多长时间.
此外, 感兴趣的描述符不局限于套接字, 任何描述符比如标准输入(fd=0)都可以用 select 来监听.

## API

	#include <sys/select.h>
	int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

### 参数
	
1. nfds, 指定被监听的描述符的总数, 通常被设置为监听的所有描述符中的最大值+1, 因为描述符从0开始计数, 0,1,2...nfds-1 将被监听.
2. readfds、writefds、exceptfds 分别指向可读、可写、异常等事件对应的文件描述符集合, 如果对某一个事件不感兴趣, 可以设为空指针. fd_set 结构体包含一个整型数组, 数组的每个元素的每一位对应一个文件描述符, 假设使用32位整数, 那么该数组的第一个元素对应描述符 0-31, 第二个元素对应 32-63, 以此类推. fd_set 能容纳的文件描述符数量由 FD_SETSIZE 指定, 通常为 1024, 这就限制了 select 能同时处理的文件描述符的总量. 为了简化对 fd_set 对象的位操作, 有以下一系列宏来访问 fd_set 结构体中的位. 
	
		#include <sys/select.h>
		FD_ZERO(fd_set *fdset);			// 清除 fdset 的所有位
		FD_SET(int fd, fd_set *fdset);		// 设置 fdset 的位 fd
		FD_CLR(int fd, fd_set *fdset);		// 清除 fdset 的位 fd
		int FD_ISSET(int fd, fd_set *fdset);// 检查 fdset 的位 fd 是否被设置
	
	例如, 我们要监测文件描述符 1、4、5 可读事件, 代码如下:

		fd_set rset;
		FD_ZERO(&rset);				
		FD_SET(1, &rset);		
		FD_SET(4, &rset);		
		FD_SET(5, &rset);		
		select(6, &rset, ...);
调用 select 时, 设置所监听的事件描述符集合中对应的标志位, 当函数返回时, 内核修改由 readfds、writefds、exceptfds 所指向的描述符集, 结果将指示哪些描述符就绪, 我们再使用 FD_ISSET 宏来检测 fd_set 数据中的描述符是否置位, 描述符集合中任何与未就绪描述符对应的位返回时均清成 0. 因此 readfds、writefds、exceptfds 是值-结果参数, 每次重新调用 select 函数时, 都得再次把描述符集中所监听的位设为 1.

		fd_set rset;
		while(1)
		{
		    FD_ZERO(&rset);
		    FD_SET(5, &rset);	// 重新设置 rset 中所监听的位对应的 fd
		    select(6, &rset, null, null, null);

		    if (FD_ISSET(6, rset) == 1)
		    {...}
		}		

3. timeout 参数用来设置 select 函数的超时时间, 它是一个 timeval 结构类型的指针, 采用指针参数是因为在正常返回时, 内核将修改它以告诉应用程序 select 等待了多长时间. 但是如果调用失败, timeout 的值是不确定的. timeval 结果定义如下:
		
		struct timeval
		{
		    long tv_sec;	// 秒数
		    long tv_usec;	// 微秒数
		}
	
	这个参数有以下三种可能:
	
	- 一直等待下去, 参数设为 null, 直到至少有一个描述符的 I/O 事件就绪才返回.
	- 等待一段固定时间, 在该时间内至少有一个描述符的 I/O 事件就绪时返回. 如果超时时间内没有任何描述符就绪, select 返回 0.
	- 不等待, 检查描述符后立即返回, 参数指向一个 timeval 结构变量, 且定时器值均为 0. 

### 返回值
- 大于 0: 就绪(可读、可写、异常)文件描述符的总数, select 调用成功时返回.
- 0: 超时时间内没有任何文件描述符就绪.
- -1: select 调用失败, 并设置 errno.

## 文件描述符就绪条件

### 可读
- Socket 内核接收缓冲区中的字节数 >= 低水位标记 SO_RCVLOWAT. 此时可以无阻塞地读该 Socket, 并且返回的字节数 > 0. 可以使用 SO_RCVLOWAT 选项设置低水位标记, 对于TCP和UDP Socket, 默认值为 1.

		注: Linux 上的 select 和 poll 系统调用并不遵守SO_RCVLOWAT的设置, 即使是1字节可用也会标记套接字可读.   
		后续的读取会被阻塞直到SO_RCVLOWAT字节数可用. 参考[http://man7.org/linux/man-pages/man7/socket.7.html]
		The select(2) and poll(2) system calls currently do not respect the SO_RCVLOWAT setting on Linux.

- Socket 通信的对方关闭连接(读半部关闭, 就是接收了 FIN 的TCP连接), 此时该 Socket 的读操作(read)将不阻塞并返回 0.
- 监听 Socket 上有新的连接请求到来, 此时对该 Socket 执行 accept调用通常不会阻塞.
- Sokcet 上有未处理的错误, 此时该 Socket 的读操作将不阻塞并返回 -1, 同时设置 errno. 也可以通过指定 SO_ERROR 套接字选项调用 getsockopt 获取并清除该错误.

### 可写
- Socket 内核发送缓冲区中的可用空间字节数 >= 低水位标记 SO_SNDLOWAT, 此时可以无阻塞地写该 Socket, 返回的字节数 > 0.可以使用 SO_SNDLOWAT 选项设置该套接字的低水位标记, 对于 TCP和UDP Socket, 默认值为 2048.
- Socket 的写操作被关闭(写半部关闭), 此时写操作将产生一个 SIGPIPE 信号.
- Sokcet 上有未处理的错误, 此时该 Socket 的读操作将不阻塞并返回 -1, 同时设置 errno. 也可以通过指定 SO_ERROR 套接字选项调用 getsockopt 获取并清除该错误.

### 异常
- Socket 上接收到带外数据或者仍处于带外标记.

### 注意
- 当 Socket 上发生错误时, 它将由 select 标记为既可读有可写.
- 接收低水位标记和发送低水位标记的目的在于: 允许应用进程控制在 select 返回时可读或可写条件之前有多少数据可读或有多大空间可用于写. 比如, 如果我们知道除非至少存在64字节的数据, 否则应用进程没有任何有效工作可做, 那么把接收低水位标记设为 64, 以防少于64个字节的数据准备好时 select 唤醒.

## select 实现

select 主要是通过三个 fd_set 记录要监测的 read/write/except 事件的文件描述符, fd_set的大小由宏 FD_SETSIZE 指定. 

### 用户空间调用过程

![](http://i.imgur.com/CrEa6sZ.png)

### 内核调用过程

![](http://i.imgur.com/FMSvxOm.png)![](http://i.imgur.com/aj3m7cZ.png)![](http://i.imgur.com/7qrXrp6.png)


1. 进入系统调用 sys_select, 使用 copy_from_user 将用户态参数拷贝到内核态, 将超时时间换成jiffies, 调用 core_sys_select.  
2. core_sys_select, 对于每个描述符read/write/except以及输入和输出, 申请6倍的映射空间, 然后从用户空间拷贝fd_set到内核空间, 调用 do_select.
3. do_select, 遍历所有fd, 调用其对应的文件系统file_operations->poll函数(对于socket, 这个poll方法是sock_poll, sock_poll根据情况会调用到 tcp_poll, udp_poll 或者 datagram_poll).
4. 以tcp_poll为例, 其核心实现就是 __pollwait, 它是注册的回调函数.
5. __pollwait的主要工作就是把 current(当前进程)挂到设备的等待队列中, 不同的设备有不同的等待队列, 对于tcp_poll来说, 其等待队列是sk->sk_sleep(注意把进程挂到等待队列中并不代表进程已经睡眠了). 在设备收到一条消息（网络设备）或填写完文件数据（磁盘设备）后, 会唤醒设备等待队列上睡眠的进程, 这时 current 便被唤醒了.
6. poll方法返回时会返回一个描述读写操作是否就绪的 mask 掩码, 根据这个mask掩码给fd_set 赋值.
7. 如果遍历完所有的fd, 还没有返回一个可读写的mask掩码, 则会调用 schedule_timeout 使调用select的进程（也就是current）睡眠, 当设备驱动发生自身资源可读写后, 会唤醒其等待队列上睡眠的进程. 如果超过一定的等待时间(schedule_timeout指定)还没有唤醒, 则调用select的进程会重新被唤醒获得CPU, 进而重新遍历fd, 判断有没有就绪的fd.
8. 把 fd_set 从内核空间拷贝到用户空间.

参考:

- [http://www.cnblogs.com/Anker/p/3265058.html](http://www.cnblogs.com/Anker/p/3265058.html)
- [http://www.cnblogs.com/apprentice89/archive/2013/05/09/3064975.html](http://www.cnblogs.com/apprentice89/archive/2013/05/09/3064975.html)
- [https://taozj.org/201604/linux-env-program-(2)-difference-select-poll-epoll.html](https://taozj.org/201604/linux-env-program-(2)-difference-select-poll-epoll.html)

### 缺点
select 的一个优点是良好的跨平台, 目前几乎在所有的平台上都支持.

- 支持的文件描述符个数有限, 由FD_SETSIZE指定, Linux上32位机默认1024个, 64位机默认是2048, 最大个数可以 cat /proc/sys/fs/file-max 查看(file-max 指定了系统范围内所有进程可以打开的文件句柄的数量限制).
- 每次调用select时, 都需要把 fd_set 从用户态拷贝到内核态, 结果也需要从内核态拷贝到用户态, 当需要监听的fd很多时开销比较大.
- 内核执行过程中, 采用轮询的方法遍历用户空间传递进来的所有fd, 如果有侦听需求就需要调用文件系统的poll调用, 返回后用户态也需要用 FD_ISSET 来依次检测到底是哪个描述符对应的bit被设置了, 如果fd_set监听的描述符比较多的话，开销也比较大.

		如果给套接字注册某个回调函数, 当他们活跃时, 自动完成相关操作, 那就避免了轮询, 这正是epoll与kqueue所实现的.
参考:
- [http://www.cnblogs.com/Anker/p/3265058.html](http://www.cnblogs.com/Anker/p/3265058.html)
- [http://www.jianshu.com/p/dfd940e7fca2](http://www.jianshu.com/p/dfd940e7fca2)


## Demo 示列

select 的使用流程图

![](http://i.imgur.com/ttuJnSI.gif)

参考:  
[http://www.cnblogs.com/Anker/archive/2013/08/14/3258674.html](http://www.cnblogs.com/Anker/archive/2013/08/14/3258674.html)
[https://www.ibm.com/support/knowledgecenter/en/ssw_i5_54/rzab6/xnonblock.htm](https://www.ibm.com/support/knowledgecenter/en/ssw_i5_54/rzab6/xnonblock.htm)

下面是一个基于 select 实现的 TCP Echo Server.
	[https://github.com/shuanghong/Linux_Net_Programming/blob/master/IO_Multiplexing/Select/src/TcpServerSelectDemo.cpp](https://github.com/shuanghong/Linux_Net_Programming/blob/master/IO_Multiplexing/Select/src/TcpServerSelectDemo.cpp)

代码中用一个数组(connectfd[FD_SETSIZE])来保存客户端接入的所有连接, 如上面缺点中所述, 用户代码仍然需要在循环中依次调用 FD_ISSET() 来检查哪个connectfd[]的bit被内核设置. 用一个变量(maxindex)来保存最大客户连接 fd 的下标, 这样可以减少调用的循环次数.  
代码中需要注意的地方:

- 每次调用 select 前要重新初始化 fd_set.

		FD_ZERO(&readfds);          // clear readfds bits
		FD_SET(listen_fd, &readfds);// enable bit everytime before call select
		...
		select(...);

- 每次调用 select 前都要在 fd_set 中使能所有有效的客户连接对应的 bit. 如果像 Unix 网络编程卷1 6.8节示列那样, 会导致 server 只服务于最新接入的客户连接, 对已有连接上的数据读写事件不会监听.

	        for (index = 0; index < FD_SETSIZE; ++index) 
        	{
	            if (connectfd[index] != -1)
	                FD_SET(connectfd[index], &readfds);
        	}

- 当有客户连接断开时, 不仅要把 fd_set 中对应的 bit清除, 也要把保存客户连接的数组中对应的 connect fd 设为初始值.

		FD_CLR(temp_connectfd, &readfds);
        for (int j = 0; j <= maxindex;  ++j) // if one connect fd closed, set arrry to -1
        {
            if (connectfd[j] == temp_connectfd)
                connectfd[j] = -1;
        }

- 增加对 select 返回的fd个数的判断, 减少不必要的 loop. 但是判断逻辑一定要至于 FD_ISSET 内, 因为只有处理了一个fd事件之后才能判断剩于 fd 的必要.

		if (FD_ISSET(temp_connectfd, &readfds))
	    {
			...
			if (--readyfd_num <= 0)
	        	break;
		}

### Demo 的缺陷
1. 非阻塞 accept
参考 Unix 网络编程卷1 16.6节, 默认情况下, Socket IO都是阻塞模式. 当使用 select 对 listen fd 监听等待一个外来连接, 新的连接接入时, select 返回并告知进程连接就绪, 随后的 accept 调用会顺利执行.  
然而, 如何客户程序调用 connect 建立连接后发送一个 RST 到服务器, 正好发生在服务器 select 和 accpet 之间, select 向进程返回可读条件, 而accept时已完成连接已经被TCP协议栈驱逐出队列, 由于没有任何已完成的连接, 服务器阻塞. 服务器进程会一直阻塞在 accept调用上, 直到其他客户建立一个新的连接, 在此期间无法处理其他已就绪的描述符.  
解决办法如下:  
a. 将 listen fd 设为非阻塞
b. 后续的 accept 调用中忽略以下错误: EWOULDBLOCK(Berkeley 实现, 客户终止连接时),ECONNABORTED (POSIX 实现, 客户终止连接时), EPROTO (SVR4 实现, 客户终止连接时), 以及 EINTR (信号被捕获).

2. 非阻塞 read/recv  
man select: [http://man7.org/linux/man-pages/man2/select.2.html](http://man7.org/linux/man-pages/man2/select.2.html)

		Under Linux, select() may report a socket file descriptor as "ready
	    for reading", while nevertheless a subsequent read blocks.  This
	    could for example happen when data has arrived but upon examination
	    has wrong checksum and is discarded.  There may be other
	    circumstances in which a file descriptor is spuriously reported as
	    ready.  Thus it may be safer to use O_NONBLOCK on sockets that should not block.

	select 返回可读, 但是并没有告知进程有多少字节可读. 并且 select 和 read/recv 是独立的系统调用, 两个操作之间存在窗口, 当调用read时数据有可能被内核丢弃(协议栈检查到这个TCP分节检验和错误, 然后丢弃), ready因无数据可读而阻塞, 导致服务器无法处理其他已就绪的描述符.  
	惊群现象: 多个进程或者线程通过 select 或者 epoll 监听一个 listen socket，当有一个新连接完成三次握手之后，所有进程都会通过 select 或者 epoll 被唤醒，但是最终只有一个进程或者线程 accept 到这个新连接，若是采用了阻塞 I/O，没有accept 到连接的进程或者线程就 block 住了
	惊群现象: 多个进程或者线程通过 select 或者 epoll 监听一个 listen fd,当有一个新连接完成三次握手之后, 所有进程都会通过 select 或者 epoll 被唤醒, 但是最终只有一个进程或者线程 accept 到这个新连接. 若是采用了阻塞 I/O, 没有accept 到连接的进程或者线程就被阻塞了.(参考: [为什么 IO 多路复用要搭配非阻塞 IO? --- 林晓峰](https://www.zhihu.com/question/37271342/answer/81757593))

	参考:  
	[为什么 IO 多路复用要搭配非阻塞 IO?](https://www.zhihu.com/question/37271342)  
	[在使用Multiplexed I/O的情况下，还有必要使用Non Blocking I/O么](https://www.zhihu.com/question/33072351).

