#include "Socket.h"

#include <arpa/inet.h>

#include <cstring>
#include <cstdio>

int main(int argc, char *argv[])  
{
    const uint32_t size = 1024;
    char buf[size] = {0};

    int connfd = 0;  
    int datalen = 0;

    struct sockaddr_in server_addr;    
    struct sockaddr_in client_addr;  

    memset(&server_addr, 0, sizeof(server_addr));   
    memset(&client_addr, 0, sizeof(client_addr));  

    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port=htons(200);  

    Socket sock(Socket::create());
    printf("Socket fd = %d\n", sock.getFd());  

    /* test move assign operator =
    Socket sock1(Socket::create());
    printf("Socket1 fd = %d\n", sock1.getFd());  

    sock = std::move(sock1);          
    printf("Socket fd = %d\n", sock.getFd());  
    printf("Socket1 fd = %d\n", sock1.getFd());  
    */

    sock.setOption(SocketOption::AddrReuse, true);
    sock.setOption(SocketOption::PortReuse, true);
    sock.bind(&server_addr);
    sock.listen(5);

    while(1)
    {
        connfd = sock.accept(&client_addr);
        if (connfd == -1)
        {
            continue;
        }

        printf("Client connected, addr:%s\n", inet_ntoa(client_addr.sin_addr));  

        while(1)
        {
            datalen = sock.receiveData(connfd, buf, size);   
            if (datalen <= 0)
            {
                printf("Receive data NULL, client disconnected!\n");  
                break;
            }
            buf[datalen] = '\0';

            printf("Receive length:%d\nReceive data:%s\n", datalen, buf);  

            if (sock.sendData(connfd, buf, datalen) == -1)
            {
                printf("Socket send failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
                break;
            }
        }
        ::close(connfd);
    }



    return 0;
}
