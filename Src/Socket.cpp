#include <fcntl.h>
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

void Socket::setOption(SocketOption op, bool on)
{
    int optval = on ? 1 : 0;

    switch (op)
    {
      case SocketOption::AddrReuse: 
      {
          if (::setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
          {
            
          }
          break;       
      }
      case SocketOption::PortReuse: 
      {
          if (::setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0)
          {
            
          }
          break;       
      }
      case SocketOption::TcpNoDelay:
      {
          if (::setsockopt(m_sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval)) < 0)
          {
            
          }
          break;       
      }
      default:
      {
        break;
      }
       
    }
}

void Socket::setNonblocking(int sockfd)
{
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    assert(flags != -1);

    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);
    assert(ret != -1);
}

void Socket::setCloseOnExec(int sockfd)
{
    int flags = ::fcntl(sockfd, F_GETFD, 0);
    assert(flags != -1);

    flags |= FD_CLOEXEC;
    int ret = ::fcntl(sockfd, F_SETFD, flags);
    assert(ret != -1);
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

Socket Socket::createTcp()
{
    std::cout << "Sock createTcp() entry" << std::endl;
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);

    /*
       int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
       setNonblocking(sockfd);
       setCloseOnExec(sockfd);
    */
    
    assert(sockfd >= 0);
    return Socket(sockfd);
}
