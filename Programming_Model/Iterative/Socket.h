#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <assert.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>


#include <utility>  // swap
#include <iostream>

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
            assert( ret == 0);
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

        swap(rhs);
        return *this;    
    }

    void swap(Socket &rhs)
    {
        std::swap(m_sockfd,  rhs.m_sockfd);
    }

    void bind(const struct sockaddr *addr);
    void listen();

    void setOption(uint32_t op, bool on);

    int recv(void *buf, int len, int flags);
    int send(const void *buf, int len, int flags);

    int getFd() const
    {
        return m_sockfd; 
    }

    static Socket create();

private:
    int m_sockfd;

};


#endif
