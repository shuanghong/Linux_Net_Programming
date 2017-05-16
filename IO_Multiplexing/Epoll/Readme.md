# IO复用之 --- Epoll
epoll 是 Linux 上特有的I/O复用函数, 在Linux2.6内核中正式引入. epoll使用一组函数来完成任务, 而不像 select, poll 是单个函数.

## System call(系统调用)

### epoll_create 创建一个epoll句柄

	#include <sys/epoll.h>
	int epoll_create(int size);
		参数 size: 在最初的实现中, size 用来告知内核调用程序希望添加到epoll实例中的文件描述符个数, 内核使用这个信息指示初始分配描述事件的内部数据结构的空间总数. 
		从Linux2.6.8以后参数被忽略, 但必须大于0, 内核动态分配所需的数据结构大小.
		返回值: 如果调用成功, 返回一个新的表示epoll实例的文件描述符, -1 则出现错误并设置 errno

    int epoll_create1(int flags);
		参数 flags： 0 --- 与epoll_create完全一样.
				EPOLL_CLOEXEC --- 对返回的 epfd 设置close-on-exec (FD_CLOEXEC) 标志, 这样做可以防止此fd泄露给执行exec后的进程.
				EPOLL_NONBLOCK --- 将返回的epfd设置为非阻塞
		返回值: 如果调用成功, 返回一个新的代表epoll实例的文件描述符, -1 则出现错误并设置 errno
更多参考 [epoll_create(2)](http://man7.org/linux/man-pages/man2/epoll_create.2.html)

### epoll_ctl 注册/修改/删除要监听的事件类型

	#include <sys/epoll.h>
	int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
	参数 epfd: epoll_create 返回的 fd
		 fd: 需要操作的目标文件描述符, 如 listenfd 或 connfd
		 op: 告知epfd对上面文件描述符fd的操作类型. 取值如下:
			 EPOLL_CTL_ADD --- 在epoll实例的事件表(参数event)中注册fd上的事件;
			 EPOLL_CTL_MOD --- 在事件表中修改fd上的注册事件; 
			 EPOLL_CTL_DEL --- 在事件表中删除fd上的注册事件.
		 event: epoll实例上fd的事件类型, 结构如下:
			struct epoll_event {
          		uint32_t     events;// 事件类型, EPOLLIN(数据可读), EPOLLOUT(数据可写), EPOLLLT(电平触发), EPOLLLT(边沿触发)
               	epoll_data_t  data; // 用户数据, union联合体, 可以保存很多类型的信息, 通常使用最多的是fd成员, 一般设为listenfd 或 connfd
           		};
			typedef union epoll_data {
               		void        *ptr;
               		int          fd;
               		uint32_t     u32;
               		uint64_t     u64;
           			} epoll_data_t;
	返回值: 0 --- 调用成功, -1 --- 调用出错并设置 errno

更多参考 [epoll_ctl(2)](http://man7.org/linux/man-pages/man2/epoll_ctl.2.html)

### epoll_wait 在一段超时时间内等待epfd上一组文件描述符上的事件发生

	#include <sys/epoll.h>
	int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
	参数 epfd: epoll_create 返回的 fd
		 fd: 需要操作的目标文件描述符, 如 listenfd 或 connfd
		 events: 结构同epoll_ctl中的event, 保存内核返回给用户程序的读写事件.
		 maxevents: 每次处理的最大事件数, 即多少个 event 结构
		 timeout: 超时时间(ms), 0 --- 调用立即返回, -1 --- 调用阻塞直到某个事件发生.
	返回值: 调用成功则返回就绪的文件描述符个数, -1 --- 调用出错并设置 errno

epoll_wait 函数如果检测到事件, 将所有的就绪事件从内核事件表(由epfd参数指定)复制到参数 events指向的结构数组中, 这个数组只用于输出epoll_wait检测到的就绪事件, 而不像 select 和 poll 的数组参数既用于传入用户注册的事件, 又用于输出内核检测到的事件, 这就极大地提供了用户程序索引就绪文件描述符的效率.  
更多参考 [epoll_wait(2)](http://man7.org/linux/man-pages/man2/epoll_wait.2.html)

## 工作模式

在 epoll_ctl 中参数 event 结构成员 events 可以是以下几个宏的集合:

- EPOLLIN: 对应的文件描述符可读(包括对端SOCKET正常关闭);
- EPOLLOUT: 对应的文件描述符可写;
- EPOLLPRI: 对应的文件描述符有紧急的数据可读(有带外数据到来);
- EPOLLERR: 对应的文件描述符发生错误;
- EPOLLHUP: 对应的文件描述符被挂断;
- EPOLLET: 将EPOLL设为边缘触发(Edge Triggered)模式, 这是相对于水平触发(Level Triggered)来说的;
- EPOLLONESHOT: 只监听一次事件,当监听完这次事件之后, 如果还需要继续监听这个socket的话, 需要再次把这个socket加入到EPOLL队列里;
- EPOLLWAKEUP: 如果EPOLLONESHOT和EPOLLET清除了, 并且进程拥有CAP_BLOCK_SUSPEND权限, 那么这个标志能够保证事件在挂起或者处理的时候, 系统不会挂起或休眠.

这里的EPOLLET标志将Epoll设为 ET 模式, 默认情况下是 LT 模式.

- LT 模式: 缺省的工作模式, 当epoll_wait检测到某描述符事件就绪并通知应用程序时, 应用程序可以不立即处理该事件; 当下次调用epoll_wait时; 内核会再次通知此事件. 即无论什么时候调用epoll_wait都会返回该就绪的描述符.  
LT 模式同时支持block和non-block socket. 内核告知你一个文件描述符是否就绪了, 然后可以对这个就绪的fd进行IO操作. 如果你不作任何操作, 内核还是会继续通知你, 所以这种模式编程出错误可能性要小一点. 传统的select/poll都是这种模型的代表, 这种模式相当于一个更高效率的poll

- ET 模式: 高速工作方式, 当epoll_wait检测到某描述符事件就绪并通知应用程序时, 应用程序必须立即处理该事件. 如果不处理, 下次调用epoll_wait时, 不会再次通知此事件.（直到你做了某些操作导致该描述符变成未就绪状态了, 也就是说边缘触发只在状态由未就绪变为就绪时通知一次, 比如描述符从unreadable变为readable或从unwritable变为writable时, epoll_wait才会返回该描述符, 如果这次不处理则下次就不会返回该描述符).  
ET模式在很大程度上减少了epoll事件被重复触发的次数, 因此效率比LT模式高. epoll工作在ET模式时必须使用non-block socket, 以避免由于一个文件描述符的阻塞读/阻塞写操作把处理多个文件描述符的任务饿死.  
更多参考 [epoll(7) Level-triggered and edge-triggered](http://man7.org/linux/man-pages/man7/epoll.7.html)

## 优点及性能提升

### 最大连接数限制

epoll 没有最大并发连接的限制, 上限是应用进程最大可以打开文件的数目. 这个数字一般远大于2048, 与系统内存关系很大, 具体可以 cat /proc/sys/fs/file-max 查看.

### 效率提升, 不随FD数目增加而线性下降
epoll 最大的优点就在于它只处理“活跃”的连接, 而跟连接总数无关.  
select 中用户程序需要轮询所有的fd_set来查找就绪的文件描述符, poll调用中需要轮询一遍pollfd才能找到对应的文件描述符, 时间复杂度为 O(n), 随着监听的描述符数量增加, 效率线性下降. 而epoll不存在这个问题, 它只会对”活跃”的socket进行操作, 这是因为在内核实现中epoll是根据每个fd上面的callback函数实现的. 只有”活跃”的socket才会主动的去调用 callback函数, 其他idle状态socket则不会. 在这点上, epoll实现了一个”伪”AIO, 因为这时候推动力在os内核.  
epoll的解决方案不像select或poll一样每次都把current轮流加入fd对应的设备等待队列中,而只在调用epoll_ctl时把current挂一遍(这一遍必不可少),并为每个fd指定一个回调函数. 当设备就绪, 唤醒等待队列上的等待者时调用这个回调函数, 而这个回调函数会把就绪的fd加入一个就绪链表, epoll_wait的工作实际上就是在这个就绪链表中查看有没有就绪的fd.

### 使用mmap加速内核与用户空间的数据传递
内存拷贝, epoll 使用了“共享内存”. 在epoll_ctl函数中, 每次注册新的事件到epoll句柄中时(op = EPOLL_CTL_ADD), 会把所有的fd拷贝进内核, 但每个fd在整个过程中只会拷贝一次, 之后不会再拷贝, 而是通过mmap同一块内存实现内核与用户空间的数据传递. select和poll需要在内核空间和用户空间来回拷贝.

### 总结
1. select, poll实现需要自己不断轮询所有fd集合, 直到设备就绪, 期间可能要睡眠和唤醒多次交替. 而epoll其实也需要调用epoll_wait不断轮询就绪链表, 期间也可能多次睡眠和唤醒交替, 但是它是设备就绪时调用回调函数, 把就绪fd放入就绪链表中, 并唤醒在epoll_wait中进入睡眠的进程. 虽然都要睡眠和交替, 但是select和poll在“醒着”的时候要遍历整个fd集合, 而epoll在“醒着”的时候只要判断一下就绪链表是否为空就行了, 这就是回调机制带来的性能提升.
2. select, poll每次调用都要把fd集合在内核空间和用户空间来回拷贝, 并且要把current往设备等待队列中挂一次, 而epoll只要一次拷贝, 而且把current往等待队列上挂也只挂一次(在epoll_wait的开始, 注意这里的等待队列并不是设备等待队列, 只是一个epoll内部定义的等待队列)这也能节省不少的开销.

参考:  
[http://blog.csdn.net/gatieme/article/details/50979090](http://blog.csdn.net/gatieme/article/details/50979090)
[http://blog.csdn.net/jiange_zh/article/details/50454726](http://blog.csdn.net/jiange_zh/article/details/50454726)

## 原理及内核实现

### 主要的数据结构

- eventpoll

struct eventpoll 是 epoll的核心结构, 执行epoll_create1时创建一个eventpoll对象, 对应一个epoll实例. 并与”epfd”和”file”关联起来, 用户态所有对epoll的操作直接通过这个关联的”epfd”进行, 而内核太则操作这个eventpoll对象.

	struct eventpoll {
		...
		wait_queue_head_t wq;			//内核函数sys_epoll_wait()中使用的等待队列, 执行epoll_wait()的当前进程(current指针)会挂在这个等待队列上
		wait_queue_head_t poll_wait;	//file->poll()中使用的等待队列, 因为eventpoll 本身也是一个file, 所以有poll操作, 在epfd本身被poll的时候有一个对应的wait_queue_head_t 队列用来唤醒上面的进程或者函数
	
		struct list_head rdllist;		//在epoll_wait 阶段, 把就绪的fd放在这个事件就绪链表上, rdllist 里面放的是epitem 这个结构
		struct rb_root rbr;				//管理每个被监听的fd(epitem)的红黑树(树根), 所有要监听的fd都在这里
		struct epitem *ovflist;			//将事件就绪的fd进行链接起来发送至用户空间
	
		struct wakeup_source *ws;
		struct user_struct *user;		//保存创建 eventpoll fd 用户信息
		struct file *file;
	
		int visited;
		struct list_head visited_list_link;
	};

	struct __wait_queue_head[wait.h]
	{
		spinlock_t		lock;
		struct list_head	task_list;
	};
	typedef struct __wait_queue_head wait_queue_head_t;

	struct list_head[types.h]
	{
		struct list_head *next, *prev;
	};

- epitem

struct epitem 是内核管理epoll所监视的fd的数据结构, 执行epoll_ctl()像系统中添加一个 fd 时, 就创建一个epitem对象. epitem与fd对应, epitem之间以rb_tree组织, tree root保存在eventpoll中. 这里使用rb_tree的原因是提高查找,插入以及删除的速度, rb_tree对以上3个操作都具有O(lgN)的时间复杂度, 后续查找某一个fd 是否有事件发生都会操作rb_tree.

	/*
	 * Each file descriptor added to the eventpoll interface will
	 * have an entry of this type linked to the "rbr" RB tree.
	 * 每个添加到 eventpoll 的 fd 都有一个 epitem
	 */
	struct epitem {
	    struct rb_node  rbn;   		// 红黑树, 保存被监视的fd
	    struct list_head  rdllink;  // 双向链表, 已经就绪的epitem(fd)都会被链到eventpoll的rdllist中
	    struct epitem  *next;
	 	struct epoll_filefd  ffd;   //保存被监视的fd和它对应的file结构, 在ep_insert()--->ep_set_ffd()中赋值
	 	int  nwait;                 //poll操作中事件的个数

	    struct list_head  pwqlist;  //双向链表, 保存被监视fd的等待队列，功能类似于select/poll中的poll_table
	    struct eventpoll  *ep;      //当前 epitem 属于哪个eventpoll, 通常一个epoll实例对应多个被监视的fd, 所以一个eventpoll结构体会对应多个epitem结构体.
	    struct list_head  fllink;   //双向链表, 用来链接被监视的fd对应的struct file. 因为file里有f_ep_link, 用来保存所有监视这个文件的epoll节点
	    struct epoll_event  event;  //注册的感兴趣的事件, epoll_ctl时从用户空间传入的 epoll_event	
	}


### system call 的内核实现
1. eventpoll_init() 函数

Linux 操作系统启动时, 内核初始化执行 eventpoll_init, 用来完成epoll相关资源的分配和初始化, 源码如下:
	
	static int __init eventpoll_init(void)
	{
		...	
		/* Allocates slab cache used to allocate "struct epitem" items */
		epi_cache = kmem_cache_create("eventpoll_epi", sizeof(struct epitem),
				0, SLAB_HWCACHE_ALIGN | SLAB_PANIC, NULL);
	
		/* Allocates slab cache used to allocate "struct eppoll_entry" */
		pwq_cache = kmem_cache_create("eventpoll_pwq",
				sizeof(struct eppoll_entry), 0, SLAB_PANIC, NULL);
	
		return 0;
	}

内核使用slab分配器在高速cache区分配内存用来存放struct epitem 和 struct ppoll_entry. 使用cache是因为epoll对所监视的fd的有大量的频繁操作

2. sys_epoll_create1() 函数

epoll_create1 系统调用的内核实现

	SYSCALL_DEFINE1(epoll_create1, int, flags)
	{
		int error, fd;
		struct eventpoll *ep = NULL;
		struct file *file;
		...
		error = ep_alloc(&ep);

		fd = get_unused_fd_flags(O_RDWR | (flags & O_CLOEXEC));

		file = anon_inode_getfile("[eventpoll]", &eventpoll_fops, ep,
					 O_RDWR | (flags & O_CLOEXEC));

		ep->file = file;
		fd_install(fd, file);
		return fd;
	}

主要工作如下:

	a. ep_alloc(&ep) 申请一个 eventpoll 对象并初始化一些重要的数据结构, 如 wq, poll_wait, rdllist.
	b. get_unused_fd_flags() 获取一个空闲的文件句柄fd
	c. anon_inode_getfile() 创建一个struct file对象(匿名文件), 将file中的struct file_operations *f_op设置为全局变量eventpoll_fops, 将void *private指向刚创建的ep, 在后面的调用中通过 file->private_data 取得eventpoll对象
	d. fd_install(fd, file) 把文件对象添加到当前进程的文件描述符表中, 实现fd与file结构绑定. [file.h]f = fdget(epfd); file = f.file
	e. 返回 fd 给用户进程使用

2. sys_epoll_ctl() 函数

epoll_ctl 系统调用的内核实现

	SYSCALL_DEFINE4(epoll_ctl, int, epfd, int, op, int, fd, struct epoll_event __user *, event)
	{
		struct eventpoll *ep;
		struct epitem *epi;
		struct epoll_event epds;
		struct eventpoll *tep = NULL;
		
		//从用户空间copy 事件类型到内核空间(struct epoll_event)
	    copy_from_user(&epds, event, sizeof(struct epoll_event)))
		f = fdget(epfd);		// 获取epfd对应的file, 见 epoll_create1
		tf = fdget(fd);			// 获取传入的需要监视的fd(如 listenfd 或 connfd)对应的file

		/* Check if EPOLLWAKEUP is allowed */
		if (ep_op_has_event(op))
			ep_take_care_of_epollwakeup(&epds);

		// 检查传入的 fd 与 epfd的file是否相等, 防止epfd嵌套, 因为epollfd本身也是个fd, 所以它本身也可以被epoll, 如果调用epoll_ctl传入的fd是epoll自身, 则形成嵌套, 有可能造成死循环
		// 检查epoll file *f_op 是否为 eventpoll_fops
		if (f.file == tf.file || !is_file_epoll(f.file))
			goto error_tgt_fput;

		ep = f.file->private_data;	// 取得eventpoll对象
		...
		epi = ep_find(ep, tf.file, fd);	//在以ep.rbr为根的红黑树中查找被监视fd对应的 epitem对象

		switch (op) {
		case EPOLL_CTL_ADD:	
			if (!epi)		//注册一个新的fd且没找到
			{		
				epds.events |= POLLERR | POLLHUP;
				error = ep_insert(ep, &epds, tf.file, fd, full_check);	//创建新的epitem对象并添加到在 eventpoll 维护的红黑树中
			} 
			else			// 注册一个新的fd但是又在红黑树中找到, 则是重复添加 fd 
				error = -EEXIST;
			if (full_check)
				clear_tfile_check_list();
			break;
		case EPOLL_CTL_DEL:
			if (epi)
				error = ep_remove(ep, epi);
			else
				error = -ENOENT;
			break;
		case EPOLL_CTL_MOD:
			if (epi) {
				epds.events |= POLLERR | POLLHUP;
				error = ep_modify(ep, epi, &epds);
			} else
				error = -ENOENT;
			break;
		}

		return error;
	}

核心工作是注册新的fd的事件类型, ep_insert()
	a. 


## Demo 示列
占坑