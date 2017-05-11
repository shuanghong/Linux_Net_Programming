# 为什么需要 IO 复用

## IO复用的需求场景
- 客户端要同时处理多个描述符, 如标准输入(stdin)和网络套接字(socket)
- 客户端要同时处理多个socket
- TCP服务器既要处理监听套接字的连接请求, 同时要处理已连接套接字的读写事件
- 服务器同时要处理TCP请求和UDP请求
- 服务器要监听多个端口或者处理多种服务.

## 服务器如何支持多个客户端的连接请求

### 仅支持单个客户端
在前一篇 Socket API 编程中([https://github.com/shuanghong/Linux_Net_Programming/tree/master/Basic_Socket_API](https://github.com/shuanghong/Linux_Net_Programming/tree/master/Basic_Socket_API)), 我们实现了一个基本的服务端程序, 其框架如下:

	int main()
	{
		...

		listen(listen_fd);   
	    while (1) 
		{
	        connect_fd = accept();
	        while (1)  {
	            request = recv(connect_fd);
	            ... 
	            send(connect_fd, response);
				...
	        }
	        close(fd);
	    }
	    ...
	}
当有一个客户端接入时, accept 执行完成, 这段代码会阻塞于 recv(), 所以它只支持一个连接, 除非这个连接断开才会处理下一个连接.

### 使用多线程/多进程支持多个客户端
如 Unix 网络编程卷1 4.8节 所述, 为每一个连接创建一个子进程, 由这个子进程为该连接提供服务. 或者创建线程, 由每个线程服务于每个连接, 即 connection per thread, 代码框架如下:

	void * SocketHandle(void* connect_fd)
	{
		...
	    while(1) 
	    {
	        recv_msg_lenth = recv(connect_fd, buf, BUFSIZE, 0);  
			...
	        send(connect_fd, buf, recv_msg_lenth, 0);
			...
	    }
	
	    close(connect_fd);
	    pthread_exit(NULL);
	}

	int main(int argc, char *argv[])  
	{
		...

		listen(listen_fd);
	    while(1)
	    {
	        connect_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &sin_size);  
			... 
	        ret = pthread_create(&thread_id, NULL, SocketHandle, &connect_fd);
			...
	    }
	    close(listen_fd); 
		
	    pthread_join(thread_id, NULL);
		...
	}

此类实现的问题在于如果有10k个连接同时在线, 就需要启动10k个进程/线程, 进程/线程的上下文切换是一个极大的消耗. 并且如果计算机只有2个CPU核心, 同一时刻只能执行2个线程, 其他线程都在等待CPU资源, 造成的结果就是系统很忙, 响应很慢.

### 一个线程如何处理多个 socket
上述第一个例子一个线程只能处理1个socket客户端, 第二个例子虽然能处理多个客户端但是要创建同等数量的线程, 那么如何才能做到一个线程能够处理多个socket呢.  
第一个例子中, 连接接入以后线程只能阻塞于 recv()等待数据的到来, recv()在数据到来之前会挂起并让出cpu直到数据到来后才能继续执行. 如果可以在这个socket数据没到来之前先处理其他socket而不是阻塞等待, 那一个线程是不是就可以处理多个socket了. 通过设置socket为非阻塞模式(O_NONBLOCK),调用recv()时就不会因为没有数据而挂起, recv() 会立即返回并在没有数据的情况下设置 errno=EWOULDBLOCK, 通过检查返回值和 errno, 便可以获知 recv()发生了什么. 代码框架如下:

	int main()
	{
	    ...
	    fcntl(listen_fd, O_NONBLOCK...); // 设置非阻塞 socket
	    listen(listen_fd); 
	
	    int fd_array[1024] = {0}; 		// 以fd为下标的数组, 存储对应的 fd
	    while (1) {
	        sleep(1); 					// 睡眠1秒, 避免cpu负载过高
	        connect_fd = accept(listen_fd); // accept一个新socket, 非阻塞, 没有连接立即返回 -1
	        if (connect_fd >= 0) {
	            ...
	            ...
	            fcntl(connect_fd, O_NONBLOCK...);	// 设置非阻塞
	            fd_array[connect_fd]= connect_fd;
	            ...
	        }
	        foreach(fd in fd_array) 	// 遍历所有accept得到的socket，调用非阻塞 recv() 尝试读取数据
			{
	            if (fd == 0)
	                continue;
	            int recv_msg_lenth = recv(fd, request); // 从socket recv一段数据
	            if (recv_msg_lenth > 0) 
				{
	                send(fd, response);		//处理数据
	            }
	            if (fd has error) 
				{
	                close(fd);
	                fd_array[fd] = 0;
	            }
	        }
	    }
	}

这段代码实现了1个线程处理多个socket的目标, 但是并不完美. 其中的 while(1)死循环会导致这个线程一直对socket轮询, 无论socket是否真的有新的连接或者数据到来, 这样会让程序总是 100% cpu满负载运转, 造成不必要的资源浪费,也会造成其他线程无法运行. 即使在处理之前 sleep 一会, 虽然可以让出cpu给其他线程使用, 但sleep太久导致socket数据不能及时处理.
  
到这, 我们终于到了问题的关键点, linux 内核为了解决这个切实的问题在实现了一系列的API, 目的就是避免忙轮询所有socket, 转而由内核主动通知哪些 socket 有数据可读, 我们在编码时也就不必为遍历socket和sleep多少秒纠结, 新的api会sleep直到某些socket有数据可读才返回, 并且直接告诉我们具体是哪些socket可读从而避免了遍历所有socket.  

linux 内核在实现这个功能的时候也经历了select, poll 两个版本的API后，才有了今天广泛使用的：kqueue(freebsd), epoll(linux).

## 经典的 C10K 问题
占坑

