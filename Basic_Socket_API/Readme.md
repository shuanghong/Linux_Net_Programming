# 基本 Socket API

## Demo

使用基本的API实现的单客户服务端 Echo 程序. 
[https://github.com/shuanghong/Linux_Net_Programming/blob/master/Basic_Socket_API/src/TcpServerBasicAPI.cpp](https://github.com/shuanghong/Linux_Net_Programming/blob/master/Basic_Socket_API/src/TcpServerBasicAPI.cpp)

## 通信流程

摘自 Unix 网络编程卷1 图4-1

![](http://i.imgur.com/NyzZ5MU.jpg)

## 基本 API
1. int socket(int family, int type, int protocol)
	
		创建一个套接字, 成功则返回一个 fd, 用于唯一标识当前的 socket. 
		参数:
			family, 指定协议族. AF_INET/PF_INET(IPv4 协议); AF_INET6/PF_INET6(IPv6 协议)
			type, 指定套接字类型. SOCK_STREAM(字节流套接字), SOCK_DGRAM(数据报套接字) 
			protocal, 指定协议类型. IPPROTO_TCP(TCP传输协议), IPPROTO_UDP(UDP传输协议), IPPROTO_SCTP(SCTP传输协议)
		返回值:
			非负的描述符: 调用成功
			-1: 调用出错, 同时设置对应的 errno

2. int bind(int sockfd, const struct sockaddr *myaddr, socklen_t addrlen)  
	
		把一个本地协议地址赋给套接字, 即套接字绑定本地服务器的 IP(32bit的IPv4地址或者128bit的IPv6地址)和端口号(16bit的TCP或者UDP端口号).
		参数:
			sockfd, 前面 socket() 返回的描述符  
			myaddr, 指针, 指向要绑定给 sockfd 的协议地址结构, 需要注意的是 ipv4和ipv6的地址结构不一样
			addrlen, 该地址结构的长度
		返回值:
			0: 调用成功
			-1: 调用出错, 同时设置对应的 errno

	* IP地址和端口选择的结果  
	![](http://i.imgur.com/VrPepSB.jpg)

	常用IP选择通配地址, 端口由用户指定, 如demo中对ipv4结构的赋值  

            server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  
            server_addr.sin_port=htons(200);    
	如果是ipv6结构, 则为

            server_addr.sin_addr.s_addr = htonl(in6addr_any);  
            server_addr.sin_port=htons(200);    

 	* 关于通配地址的解释

			INADDR_ANY 即地址为 0.0.0.0, 表示对本地所有网卡(服务器可能不止一个网卡)的所有ip(一个网卡可能多个ip)地址都进行绑定.  
			也就是说可以收到发送到所有有效地址上的数据包.  

3. int listen(int sockfd, int backlog)

		作为服务器, 在调用socket(), bind()之后就会调用listen()来监听这个socket.  
		socket()函数创建的socket默认是一个主动类型的, listen()函数将socket变为被动类型, 等待客户的连接请求.

 	* 关于参数 backlog 的解释  
	不同的Unix系统对backlog的解释是不一样的, 内核为任何一个给定的监听套接字维护两个队列  
	1). 未完成的连接队列, 每个SYN分节对应其中一项, 此分节已从客户端发出并到达服务器, 而服务器正在等待TCP三次握手协议的完成, socket处于 SYN_RCVD 状态.  
	2). 已完成的连接队列, 每个已完成TCP三次握手过程的客户对应其中一项, socket处于 ESTABLISHED状态.  
	如下图  
		![](http://i.imgur.com/6WHJef6.jpg)
		![](http://i.imgur.com/p2k7jVh.jpg)
  
	图3-2中显示在客户端connect()期间, TCP的三次握手过程. 当客户端的SYN分节到达Server时, TCP在未完成连接队列中创建一个新项; 直到第三次分节到达Server或者超时, 该项从未完成的队列转移到已完成队列.  
	对于未完成的队列, 有一种情况是可能发生的, 即客户端会在服务器执行accept()之前调用conncet(), 此时服务器可能正忙于处理其他客户端, 未响应客户端的SYN, 这将会产生一个未完成的连接.  
	backlog 参数用来限制这种未连接的数量, 在这个限制之内的连接请求会立即成功, 之外的连接请求就会阻塞直到被Server的accept()处理,并从未连接队列删除. 如下图  

	![](http://i.imgur.com/jVvC4Gm.jpg)

	Linux man 关于 backlog的解释, 参考[man 2 listen](http://man7.org/linux/man-pages/man2/listen.2.html)  

        The backlog argument defines the maximum length to which the queue of pending connections for sockfd may grow.  
		If a connection request arrives when the queue is full, the client may receive an error with an indication 
		of ECONNREFUSED or, if the underlying protocol supports retransmission, the request may be ignored so that 
		a later reattempt at connection succeeds.
    
4. int accept(int sockfd, const struct sockaddr *cliaddr, socklen_t *addrlen)

		当客户端调用 connect 发起连接时, 服务端调用 accept() 用于从内核中已完成的队列队头返回下一个已完成的连接, 如上图 3-1. 如果已完成的队列为空, 则进程进入睡眠(Socket 默认为阻塞模式).
		参数:
			sockfd, 监听套接字(listen socket fd), 由socket()函数创建, 用作 bind和linten的第一个参数  
			cliaddr, 指针, 指向已连接的客户端的协议地址
			addrlen, 地址结构的长度, 值-结果参数, 调用时其为cliaddr所指向的socket地址结构的长度, 返回时为内核存放在该socket地址结构内的实际字节数
		返回值:
			调用成功返回一个新的已连接的套接字描述符(connection fd), 代表着与客户端的 TCP 连接.
			-1: 调用出错, 同时设置对应的 errno

## 网络 I/O 函数

当客户端和服务器建立连接后, 可以使用	网络I/O进行读写操作, 网络I/O有如下几组:

	1. read()/write()
	2. recv()/send()
	3. readv()/writev()
	4. recvmsg()/sendmsg()
	5. recvfrom()/sendto()

- ssize_t recv(int sockfd, void *buf, size_t len, int flags)

		数据接收的大致流程: 内核接受远端数据--->存到Linux TCP协议接收缓存区---> copy到用户空间buf(同时要清除协议缓存区).  
		一般 len 为用户程序指定的buf大小, recv此类的I/O并非真正的读操作,只是将内核接收缓冲区数据copy到用户空间, 实际数据接收由内核协议栈完成.  
		如果将接收缓冲区大小设为 0, recv将直接从协议缓冲区(滑动窗口区)读取数据, 避免了数据从协议缓冲区到接收缓冲区的拷贝.  
		参数:
			sockfd, accept()调用返回的 connection socket fd
			buf, 用户程序指定的保存接收数据的缓冲区
			len, 请求读取的数据长度, 一般设为缓冲区的大小
			flags, 标志位. MSG_OOB: 读取带外数据; MSG_PEEK:从缓冲区返回数据但不清空缓冲区; 
					MSG_WAITALL: 告知内核不要在尚未读入请求数目的字节之前返回(当发生错误时仍然有可能返回比请求字节数要少的数据)
		返回值:
			调用成功返回一个实际读到的字节数
			0: 客户端连接关闭
			-1: 调用出错, 同时设置对应的 errno
	
	* 关于缓冲区的长度 len, 多大合适?  
	对于数据流套接字(SOCK_STREAM, TCP), 如果应用层协议是交互式的, 选择一个能保存最大消息/命令的buf大小. 如果应用层协议要发送大块数据, 则buf大小越大越好, 大约等于内核接收缓冲区的大小.
	对于数据包套接字(SOCK_DGRAM, UDP), buf 大小足够能保存应用层最大的数据包. 通常应用层协议不应该发送超过 1400字节的数据包, 因为超过之后数据会被分片和重组.
  
		参考: [http://stackoverflow.com/questions/2862071/how-large-should-my-recv-buffer-be-when-calling-recv-in-the-socket-library](http://stackoverflow.com/questions/2862071/how-large-should-my-recv-buffer-be-when-calling-recv-in-the-socket-library)

	* recv 何时返回, 内核缓冲区数据实际大小对recv的影响   
	recv调用时, 我们告知内核想要读取的数据大小, 实际上内核接收缓冲区的数据长度未知. 当缓冲区没有数据时, 进程睡眠(默认阻塞模式下). 当接收缓冲区数据高于最低水位标志(SO_RCVLOWAT, 默认为1字节) 时, recv 认为可读.  
	如果缓冲区数据比buf大, 则(TCP Socket)填满buffer后recv函数返回, 内核操作系统将剩下的数据排队,等待下一次调用 recv. 用户程序循环读取直到返回 EAGAIN,表示读完.
	(UDP Socket)超出buf的数据会被丢弃.   
	如果缓冲区数据比buf小, 则取走缓存区中的所有数据后recv函数返回, 所以recv的策略是有多少读多少. 
	
	* 如何才能知道接收到了完整的数据消息   
	TCP Socket, 在应用层协议建立一种判断消息结束的规则, 比如每个消息带有特殊的结束符, 或者消息中带有消息长度的字段.  
	UDP Socket, 单次调用 recv总是返回单个数据报.

	* 关于recv和TCP粘包问题, 参考  
	[http://www.cnblogs.com/QG-whz/p/5537447.html](http://www.cnblogs.com/QG-whz/p/5537447.html)   
	[http://www.tuicool.com/articles/vaE3iq](http://www.tuicool.com/articles/vaE3iq)  
	[ http://www.cnblogs.com/diegodu/p/4538210.html]( http://www.cnblogs.com/diegodu/p/4538210.html)  
	[http://blog.csdn.net/zlzlei/article/details/7689409](http://blog.csdn.net/zlzlei/article/details/7689409)

	* 关于阻塞/非阻塞模式下的recv行为, 参考  
	[http://blog.csdn.net/houlaizhe221/article/details/6580775](http://blog.csdn.net/houlaizhe221/article/details/6580775)  
	[http://blog.csdn.net/russell_tao/article/details/9950615](http://blog.csdn.net/russell_tao/article/details/9950615)