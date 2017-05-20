# Epoll 的内核实现

## 主要的数据结构

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

	    struct list_head  pwqlist;  //双向链表, 保存被监视fd的等待队列
	    struct eventpoll  *ep;      //当前 epitem 属于哪个eventpoll, 通常一个epoll实例对应多个被监视的fd, 所以一个eventpoll结构体会对应多个epitem结构体.
	    struct list_head  fllink;   //双向链表, 用来链接被监视的fd对应的struct file. 因为file里有f_ep_link, 用来保存所有监视这个文件的epoll节点
	    struct epoll_event  event;  //注册的感兴趣的事件, epoll_ctl时从用户空间传入的 epoll_event	
	}

- eppoll_entry

一个 epitem 关联到一个eventpoll, 就会有一个对应的struct eppoll_entry. eppoll_entry 主要完成epitem和epitem 事件发生时的callback 函数之间的关联.  
在ep_ptable_queue_proc函数中, 创建 eppoll_entry 对象, 设置等待队列中的回调函数为ep_poll_callback, 然后加入设备等待队列(将eppoll_entry 对象到epitem的等待队列”pwqlist”). 当设备就绪时，会唤醒等待队列上的进程，同时会取出等待队列上的eppoll_entry元素，调用其回调函数，对于epoll来说，回调函数为ep_poll_callback

	struct eppoll_entry {
		/* List header used to link this structure to the "struct epitem" */
		struct list_head llink;
	
		/* The "base" pointer is set to the container "struct epitem" */
		struct epitem *base;
	
		/*
		 * Wait queue item that will be linked to the target file wait queue head.
		 */
		wait_queue_t wait;
	
		/* The wait queue head that linked the "wait" wait queue item */
		wait_queue_head_t *whead;
	};


## system call 的内核实现

### eventpoll_init() 函数

Linux 操作系统启动时, 内核初始化执行 eventpoll_init, 用来完成epoll相关资源的分配和初始化, 源码如下:
	
	static int __init eventpoll_init(void)
	{
		...	
		/* Allocates slab cache used to allocate "struct epitem" items 存放 epitem 的cache */
		epi_cache = kmem_cache_create("eventpoll_epi", sizeof(struct epitem),
				0, SLAB_HWCACHE_ALIGN | SLAB_PANIC, NULL);	
	
		/* Allocates slab cache used to allocate "struct eppoll_entry" 存放eppoll_entry的cache*/
		pwq_cache = kmem_cache_create("eventpoll_pwq",
				sizeof(struct eppoll_entry), 0, SLAB_PANIC, NULL);
	
		return 0;
	}

内核使用slab分配器在高速cache区分配内存用来存放struct epitem 和 struct ppoll_entry. 使用cache是因为epoll对所监视的fd的有大量的频繁操作.


### sys_epoll_create1() 函数

epoll_create1 系统调用的内核实现

	SYSCALL_DEFINE1(epoll_create1, int, flags)
	{
		int error, fd;
		struct eventpoll *ep = NULL;
		struct file *file;
		...
		error = ep_alloc(&ep);

		fd = get_unused_fd_flags(O_RDWR | (flags & O_CLOEXEC));
		file = anon_inode_getfile("[eventpoll]", &eventpoll_fops, ep, O_RDWR | (flags & O_CLOEXEC));

		ep->file = file;
		fd_install(fd, file);
		return fd;
	}

主要工作如下:

	a. ep_alloc(&ep) 申请一个 eventpoll 对象并初始化一些重要的数据结构, 如 wq, poll_wait, rdllist.
	b. get_unused_fd_flags() 获取一个空闲的文件句柄fd, anon_inode_getfile() 创建一个struct file对象(匿名文件), 将file中的struct file_operations *f_op设置为全局变量eventpoll_fops, 将void *private指向刚创建的ep, 在后面的调用中通过 file->private_data 取得eventpoll对象
	c. fd_install(fd, file) 把文件对象添加到当前进程的文件描述符表中, 实现fd与file结构绑定. [file.h]f = fdget(epfd); file = f.file
	d. 返回 fd 给用户进程使用

执行完成后的数据结构如下:

![](http://i.imgur.com/n0tQU2L.png)


### sys_epoll_ctl() 函数

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
		epi = ep_find(ep, tf.file, fd);	//在以ep.rbr为根的红黑树中查找被监视fd对应的epitem对象

		switch (op) {
		case EPOLL_CTL_ADD:	
			if (!epi)		//注册一个新的fd且红黑树中没找到 epitem
			{		
				epds.events |= POLLERR | POLLHUP;
				error = ep_insert(ep, &epds, tf.file, fd, full_check);	//创建新的epitem对象并添加到在 eventpoll 维护的红黑树中
			} 
			else			//注册一个新的fd但是又在红黑树中找到, 则是重复添加 fd 
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

针对注册新fd的操作, 核心是 ep_insert(), 大致流程如下:

![](http://i.imgur.com/J8tq0Sn.jpg)


1. ep_insert()

		static int ep_insert(struct eventpoll *ep, struct epoll_event *event, struct file *tfile, int fd, int full_check)		
		{
			struct epitem *epi;
			struct ep_pqueue epq;
	
			epi = kmem_cache_alloc(epi_cache, GFP_KERNEL))
	
			/* Item initialization follow here ... */
			INIT_LIST_HEAD(&epi->rdllink);
			INIT_LIST_HEAD(&epi->fllink);
			INIT_LIST_HEAD(&epi->pwqlist);
			epi->ep = ep;
			ep_set_ffd(&epi->ffd, tfile, fd);
			epi->event = *event;
			epi->nwait = 0;
			epi->next = EP_UNACTIVE_PTR;
			...
			epq.epi = epi;
			init_poll_funcptr(&epq.pt, ep_ptable_queue_proc);
	
			revents = ep_item_poll(epi, &epq.pt);
		
			list_add_tail_rcu(&epi->fllink, &tfile->f_ep_links);
			
			ep_rbtree_insert(ep, epi);	// epitem对象插入到 eventpoll 的红黑树中
				
			/* If the file is already "ready" we drop it inside the ready list 
			如果此时有用户关心的事件发生并且 epitem对象的就绪队列不为空
			*/
			if ((revents & event->events) && !ep_is_linked(&epi->rdllink)) {
				// 当前的epitem对象链到 eventpoll的就绪队列上
				list_add_tail(&epi->rdllink, &ep->rdllist);
				ep_pm_stay_awake(epi);
		
				//唤醒调用 epoll_wait的进程, 执行epoll_wait()中注册在等待队列上的回调函数
				if (waitqueue_active(&ep->wq))
					wake_up_locked(&ep->wq);
				//唤醒 epoll 当前的 epollfd的进程
				if (waitqueue_active(&ep->poll_wait))
					pwake++;
			}
			return 0;
		}

2. 	init_poll_funcptr(&epq.pt, ep_ptable_queue_proc)

设置 ep_pqueue 对象成员pt中的回掉函数qproc为 ep_ptable_queue_proc

		init_poll_funcptr(poll_table *pt, poll_queue_proc qproc)
		{
			pt->_qproc = qproc;
			pt->_key   = ~0UL; 
		}

3. ep_item_poll() 

调用被监视fd的poll函数, 查看当前fd的事件状态位.  
如果fd是套接字, 则f_op为 socket_file_ops, poll函数是sock_poll() --- 参见 socket.c 中的函数 sock_alloc_file()  
如果是TCP socket, 则调用顺序为: sock_poll()-->tcp_poll()-->sock_poll_wait()-->poll_wait()-->epq.pt.qproc[ep_ptable_queue_proc] 
	
		ep_item_poll(struct epitem *epi, poll_table *pt)
		{
			pt->_key = epi->event.events;
			// fd 当前的事件 & 用户事件, 用来判断是否已有用户关心的事件发生
			return epi->ffd.file->f_op->poll(epi->ffd.file, pt) & epi->event.events;
		}

此时的数据结构关系如下:
![](http://i.imgur.com/bFZRPdO.jpg)


4. ep_ptable_queue_proc()

参数: file 由上面的epi->ffd.file传入, 是被监视fd对应的file结构, whead 是该fd对应的设备等待队列, pt 则是ep_pqueue 对象的成员 struct poll_table pt.  

		static void ep_ptable_queue_proc(struct file *file, wait_queue_head_t *whead, poll_table *pt)
		{
			// ep_pqueue 对象包含epitem对象和poll_table, 由pt取得epitem对象
			struct epitem *epi = ep_item_from_epqueue(pt);
			struct eppoll_entry *pwq;
			// 创建 eppoll_entry 对象
			pwq = kmem_cache_alloc(pwq_cache, GFP_KERNEL)
			// 设置 eppoll_entry 对象的等待队列的回调函数为 ep_poll_callback
			init_waitqueue_func_entry(&pwq->wait, ep_poll_callback);

			pwq->whead = whead;	// eppoll_entry 对象的whead为fd的设备等待队列
			pwq->base = epi;	
			add_wait_queue(whead, &pwq->wait);// 添加eppoll_entry的等待队列到fd的等待队列中
			list_add_tail(&pwq->llink, &epi->pwqlist);// 添加eppoll_entry的llink到epitem对象的pwqlist
			epi->nwait++;
		}

add_wait_queue(whead, &pwq->wait);   
是非常关键的一步. 对于 tcp socket, 这里的 whead 由 sock_poll_wait()传入, 是sk_sleep()的返回结果, 是 strcut sock (sock.h)成员 sk_wq 结构中的wait值, 代码如下:

	static inline wait_queue_head_t *sk_sleep(struct sock *sk)
	{
		BUILD_BUG_ON(offsetof(struct socket_wq, wait) != 0);
		return &rcu_dereference_raw(sk->sk_wq)->wait;
	}

	struct socket_wq 
	{
		wait_queue_head_t	wait;
		struct fasync_struct	*fasync_list;
		unsigned long		flags; 
		struct rcu_head		rcu;
	} ____cacheline_aligned_in_smp;

strcut sock 成员 sk_wq 在 sock_init_data()函数中赋值为 strcut socket 成员 wq.  
sock_init_data()的调用流程是 sock_create()-->__sock_create()-->pf->create()[af_inet.c/af_inet6.c 中初始化]-->inet_create()-->sock_init_data(), 这也是在创建一个 socket 的大致流程.  
而socket结构对象在 __sock_create()-->sock_alloc()-->SOCKET_I() 取得, 根据 socket_alloc 中的成员 vfs_inode, 取 socket.

	struct socket_alloc 
	{
		struct socket socket;
		struct inode vfs_inode;
	};

	static inline struct socket *SOCKET_I(struct inode *inode)
	{
		return &container_of(inode, struct socket_alloc, vfs_inode)->socket;//获取一个 socket 
	}

而 socket 则在 sock_alloc_inode()创建, 并且初始化了 socket.wq.

	static struct inode *sock_alloc_inode(struct super_block *sb)
	{
		struct socket_alloc *ei;
		struct socket_wq *wq;
	
		ei = kmem_cache_alloc(sock_inode_cachep, GFP_KERNEL);	// 创建socket_alloc 对象, 并初始化 socket 
		if (!ei)
			return NULL;
		wq = kmalloc(sizeof(*wq), GFP_KERNEL);
		if (!wq) {
			kmem_cache_free(sock_inode_cachep, ei);
			return NULL;
		}
		init_waitqueue_head(&wq->wait);
		wq->fasync_list = NULL;
		wq->flags = 0;
		RCU_INIT_POINTER(ei->socket.wq, wq);
	
		ei->socket.state = SS_UNCONNECTED;
		ei->socket.flags = 0;
		ei->socket.ops = NULL;
		ei->socket.sk = NULL;
		ei->socket.file = NULL;
	
		return &ei->vfs_inode;
	}
	
至此, whead <--- strcut sock.sk_wq(->wait) <--- struct socket.wq(->wait), 这便是创建一个socket时fd的等待队列.
	
	struct socket {
		struct socket_wq __rcu	*wq;
		struct file		*file;
		struct sock		*sk;
		const struct proto_ops	*ops;
	};
	
	struct socket_wq 
	{
		wait_queue_head_t	wait;
		...
	} ____cacheline_aligned_in_smp;

到这的数据结构关系如下:

![](http://i.imgur.com/POw1lyL.jpg)

	
当硬件设备有数据到来时, 硬件中断处理函数会唤醒该等待队列上等待的进程时执行该fd 等待队列的回调函数, 把当前这个进程给唤醒.   
以 tcp socket为例, 当fd上有事件发生时, 如客户端发起connect(), 服务端收到客户端ACK, 调用流程 tcp_v4_rcv()-->tcp_v4_do_rcv()-->tcp_child_process()-->parent->sk_data_ready(parent)[sock_def_readable]-->wake_up_interruptible_sync_poll()-->__wake_up_sync_key()-->__wake_up_common()-->curr->func(对于加入epoll的fd而言即为 ep_poll_callback).

	调用流程参考:
	http://blog.csdn.net/zhangskd/article/details/45770323
	http://blog.csdn.net/a364572/article/details/40628171

	如果是客户端send(), 则调用流程: tcp_v4_rcv()-->tcp_v4_do_rcv()-->tcp_rcv_established()-->tcp_data_queue()-->sk->sk_data_ready(sk)[sock_def_readable]...-->ep_poll_callback()
	
唤醒主要分两种情况: 唤醒注册时候的进程, 让注册的进程重新执行, 比如在epoll_wait 的时候对应的唤醒函数就是唤醒这个执行 epoll_wait 的这个进程; 或者唤醒的时候执行注册的某一个函数.

5. ep_poll_callback

		static int ep_poll_callback(wait_queue_t *wait, unsigned mode, int sync, void *key)
		{
			// 由 eppoll_entry 对象中的wait取得base指向的epitem对象 epi
			struct epitem *epi = ep_item_from_wait(wait);
			struct eventpoll *ep = epi->ep;
			...
		
			/*
			 * If we are transferring events to userspace, we can hold no locks (because we're accessing user memory, and because of linux f_op->poll() semantics). All the events that happen during that period of time are chained in ep->ovflist and requeued later on.
			 * 如果该callback被调用的同时, epoll_wait()已经返回了,
			 * 也就是说, 此刻应用程序有可能已经在循环获取events,
			 * 这种情况下, 内核将此刻发生event的epitem用一个单独的链表链起来, 不发给应用程序, 也不丢弃, 而是在下一次epoll_wait时返回给用户
			 */
			if (unlikely(ep->ovflist != EP_UNACTIVE_PTR)) {
				if (epi->next == EP_UNACTIVE_PTR) {
					epi->next = ep->ovflist;
					ep->ovflist = epi;
					if (epi->ws) {
						/*
						 * Activate ep->ws since epi->ws may get
						 * deactivated at any time.
						 */
						__pm_stay_awake(ep->ws);
					}
		
				}
				goto out_unlock;
			}
		
			/* If this file is already in the ready list we exit soon */
			// 将当前的epitem 的就绪表链到 eventpoll的就绪链表(如果已存在则什么也不做), 在后续的epoll_wait中会检查eventpoll的rdllist是否为空
			if (!ep_is_linked(&epi->rdllink)) {
				list_add_tail(&epi->rdllink, &ep->rdllist);
				ep_pm_stay_awake_rcu(epi);
			}
		
			/*
			 * Wake up ( if active ) both the eventpoll wait list and the ->poll() wait list.
			 */
			// 唤醒epoll_wait中等待队列上注册的回调函数 */
			if (waitqueue_active(&ep->wq))
				wake_up_locked(&ep->wq);
			if (waitqueue_active(&ep->poll_wait))
				pwake++;
		
			return 1;
		}