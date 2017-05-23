#include "Socket.h"


void Socket::bind(const struct sockaddr_in *addr)
{
    int ret = ::bind(m_sockfd, (struct sockaddr*)(addr), sizeof(struct sockaddr));
}

void Socket::listen(const int backlog)
{
    int ret = ::listen(m_sockfd, backlog);
}


int Socket::accept(const struct sockaddr_in *addr)
{
    socklen_t addrlen = sizeof(*addr);

    int connfd = ::accept4(m_sockfd, (struct sockaddr*)(addr), &addrlen, SOCK_CLOEXEC);
    //int connfd = ::accept(m_sockfd, (struct sockaddr*)(addr), &addrlen);

    return connfd;
}

void Socket::setOption(uint32_t op, bool on)
{

}

int Socket::receiveData(int connectfd, void *buf, int len)
{
    int flags = 0;
    return ::recv(connectfd, buf, len, flags);
}

int Socket::sendData(int connectfd, const void *buf, int len)
{
    int flags = 0;
    return ::send(connectfd, buf, len, flags);
}

Socket Socket::create()
{
    std::cout << "Sock create() entry" << std::endl;
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);

    assert(sockfd >= 0);
    return Socket(sockfd);
}
