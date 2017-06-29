#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <assert.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <utility>  // swap
#include <iostream>

enum class SocketOption: uint32_t
{
    AddrReuse = 0,
    PortReuse,
    TcpNoDelay,
    Invalid
};

class Socket
{
public:
    explicit Socket(int sockfd)
    :m_sockfd(sockfd)
    {
        std::cout << "Sock contructor(int) called entry" << std::endl;
        assert(m_sockfd >= 0);
    }

    ~Socket()
    {
        if (m_sockfd >=0)
        {
            int ret = ::close(m_sockfd);
            assert(ret == 0);
        }
    }
    
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket &&rhs)
    {
        std::cout << "Sock move contructor called entry" << std::endl;

        m_sockfd = rhs.m_sockfd;
        assert(m_sockfd >= 0);

        rhs.m_sockfd = -1;
    }

    Socket& operator=(Socket &&rhs)
    {
        std::cout << "Sock move assinment opeartor called entry" << std::endl;

        //swap(rhs);
        int ret = ::close(m_sockfd);
        assert(ret == 0);

        m_sockfd = rhs.m_sockfd;
        rhs.m_sockfd = -1;
        return *this;    
    }

    void swap(Socket &rhs)
    {
        std::swap(m_sockfd,  rhs.m_sockfd);
    }

    void setOption(SocketOption op, bool on);
    void setNonblocking(int sockfd);
    void setCloseOnExec(int sockfd);

    void bind(const struct sockaddr_in *addr);
    void listen(const int backlog = SOMAXCONN);
    int accept(const struct sockaddr_in *addr);
    int receiveData(int connectfd, void *buf, int len);
    int sendData(int connectfd, const void *buf, int len);

    int getFd() const
    {
        return m_sockfd; 
    }

    static Socket createTcp();

private:
    int m_sockfd;

};


#endif
