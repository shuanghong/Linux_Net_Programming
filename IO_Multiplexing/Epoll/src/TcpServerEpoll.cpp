#include <arpa/inet.h>
#include <sys/epoll.h>

#include <cstring>
#include <cstdio>

#include "../../../Src/Socket.h"

const uint32_t kBufSize = 1024;

void handleAccept(int epollfd, Socket &sk)
{
    struct sockaddr_in client_addr;  
    memset(&client_addr, 0, sizeof(client_addr));  

    int connfd = sk.accept(&client_addr);
    if (connfd == -1)
    {
        printf("Accept error!");  
        return;
    }

    printf("Client connected, addr:%s, port:%d \n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));  

    struct epoll_event ev;
    ev.data.fd = connfd;
    ev.events = EPOLLIN;         // default LT mode for connfd read event

    epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev);
}

void handleRead(int epollfd, int connfd, Socket &sk, char *buf)
{
    struct epoll_event ev;
    ev.data.fd = connfd;

    memset(buf, 0, kBufSize);
    int datalen = sk.receiveData(connfd, buf, kBufSize);   

    if (datalen <= 0)
    {
        printf("Receive data NULL, client disconnected!\n");  

        ev.events = EPOLLIN;         
        epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, &ev); // delete connfd read event
        close(connfd);
    }
    else
    {
        buf[datalen] = '\0';
        printf("Receive length:%d\nReceive data:%s\n", datalen, buf);  

        ev.events = EPOLLOUT;         // default LT mode for connfd write event
        epoll_ctl(epollfd, EPOLL_CTL_MOD, connfd, &ev); // modify interest event from read to write
    }

}

void handleWrite(int epollfd, int connfd, Socket &sk, char *buf)
{
    struct epoll_event ev;
    ev.data.fd = connfd;

    if (sk.sendData(connfd, buf, strlen(buf)) == -1)
    {
        printf("Socket send failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  

        ev.events = EPOLLOUT;         
        epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, &ev); // delete connfd write event
    }
    else
    {
        printf("Send length:%d\nSend data:%s\n", strlen(buf), buf);  
        ev.events = EPOLLIN;         // default LT mode for connfd read event
        epoll_ctl(epollfd, EPOLL_CTL_MOD, connfd, &ev); // modify interest event from write to read
    }
}

int main(int argc, char *argv[])  
{
    char buf[kBufSize] = {0};

    struct sockaddr_in server_addr;    
    memset(&server_addr, 0, sizeof(server_addr));   

    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port=htons(200);  

    Socket sock(Socket::createTcp());
    printf("Socket fd = %d\n", sock.getFd());  

    sock.setOption(SocketOption::AddrReuse, true);
    sock.setOption(SocketOption::PortReuse, true);
    sock.bind(&server_addr);
    sock.listen(5);

    const uint32_t kMaxEvents = 20;
    struct epoll_event events[kMaxEvents];
    struct epoll_event ev;

    int epfd = epoll_create1(0);

    ev.data.fd = sock.getFd();   
    ev.events = EPOLLIN;         // default LT mode for listenfd
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock.getFd(), &ev);

    while(1)
    {
        int numfds = epoll_wait(epfd, events, kMaxEvents, -1);

        printf("Ready fd numbers: %d\n", numfds);  

        for (int i = 0; i < numfds; ++i)
        {
            int sockfd = events[i].data.fd;

            if (sockfd == sock.getFd() && (events[i].events & EPOLLIN))
            {
                handleAccept(epfd, sock);
            }
            else if (events[i].events & EPOLLIN)
            {
                handleRead(epfd, sockfd, sock, buf);
            }
            else if (events[i].events & EPOLLOUT)
            {
                handleWrite(epfd, sockfd, sock, buf);
            }
            else
            {
                printf("Unknown happened events: %d", events[i].events);
            }
        }
    }

    ::close(epfd);

    return 0;
}
