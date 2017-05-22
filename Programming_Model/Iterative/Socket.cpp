#include "Socket.h"


void Socket::bind(const struct sockaddr *addr)
{
    int ret = ::bind(m_sockfd, addr, sizeof(*addr));
}

void Socket::listen()
{
    int ret = ::listen(m_sockfd, 5);
}

void Socket::setOption(uint32_t op, bool on)
{

}

int Socket::recv(void *buf, int len, int flags)
{
    return 0;
}

int Socket::send(const void *buf, int len, int flags)
{
    return 0;
}

Socket Socket::create()
{
    std::cout << "Sock create() entry" << std::endl;
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);

    assert(sockfd >= 0);
    return Socket(sockfd);
}
