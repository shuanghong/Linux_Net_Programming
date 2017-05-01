#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024 

const static char *welcome_msg = "Welcome to socket server";

char buf[BUFSIZE] = {0};    
int recv_msg_lenth = 0;  
int listen_fd = 0;   
int connect_fd = 0;  
socklen_t sin_size = 0u;  

struct sockaddr_in server_addr;    
struct sockaddr_in client_addr;  

void createSocket()
{
    memset(&server_addr, 0, sizeof(server_addr));   
    memset(&client_addr, 0, sizeof(client_addr));  

    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port=htons(200);  
      
    if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)  
    {    
        printf("Socket create failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        return;
    }  

    int reuse_addr_opt = 1;
    if ( setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt)) == -1 )
    {
        printf("Socket set reuse addr option failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        return;
    }

    int reuse_port_opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &reuse_port_opt, sizeof(reuse_port_opt)) == -1)
    {
        printf("Socket set resue port option failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        return;
    }
   
}

void bindAndListenSocket()
{
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)  
    {  
        printf("Socket bind failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        return;
    }  
      
    if (listen(listen_fd, 5) == -1)
    {
        printf("Socket listen failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        return;
    } 

}


int main(int argc, char *argv[])  
{  
    createSocket();
    bindAndListenSocket();

    sin_size = sizeof(struct sockaddr_in);  

    int maxfd = listen_fd;
    int connectfd[FD_SETSIZE] = {-1};       // thhis can not set all data to -1, refer to http://bbs.csdn.net/topics/360017909
    memset(connectfd, -1, sizeof(connectfd));

    int index = 0;
    int maxindex  = -1;
    int temp_connectfd = 0;
      
    fd_set readfds;
    
    while(1)    
    {
        FD_ZERO(&readfds);          // clear readfds bits
        FD_SET(listen_fd, &readfds);    // enable bit everytime before call select

        for (index = 0; index < FD_SETSIZE; ++index)     // check data on all exist connection fd
        {
            if (connectfd[index] != -1)
                FD_SET(connectfd[index], &readfds);                // add all connectfd to readfds for next select call
        }

        int readyfd_num = select(maxfd + 1, &readfds, nullptr, nullptr, nullptr);    //wait for read event, write/exception are ignore

        switch (readyfd_num)
        {
            case 0:
            {
                printf("Select timeout!");

                continue;
            }
            case -1:
            {
                printf("Select failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
                exit(1);
            }

            default:
            {
                if (FD_ISSET(listen_fd, &readfds))  // new client connection
                {
                    connect_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &sin_size);  
                    if (connect_fd == -1)
                    {  
                        printf("Socket accept failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
                        continue;
                    }  

                    for (index = 0; index < FD_SETSIZE; ++index)
                    {
                        if (connectfd[index] == -1)
                        {
                            connectfd[index] = connect_fd;      // store connection fd
                            break;
                        }
                    }

                    if (index > maxindex)
                    {
                        maxindex = index;                       // update maxindex if needed
                    }

                    //FD_SET(connect_fd, &readfds);                // add new bit of connectfd to readfds

                    if (connect_fd > maxfd)                     // update maxfd if needed
                    {
                        maxfd = connect_fd;                     
                    }
                    printf("New client connected, addr :%s:%d\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);  
                    if (send(connect_fd, welcome_msg, strlen(welcome_msg), 0) == -1)
                    {
                        printf("Socket send welcome msg failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
                        continue;
                    
                    }  
                    send(connect_fd, "\r\n>", strlen("\r\n>"), 0);  
                }

                for (index = 0; index <= maxindex; ++index)     // check data on all exist connection fd
                {
                    printf("index: %d, maxindex: %d\n", index, maxindex);
                    if ((temp_connectfd = connectfd[index]) == -1)
                        continue;

                    if (FD_ISSET(temp_connectfd, &readfds))
                    {
                        recv_msg_lenth = recv(temp_connectfd, buf, BUFSIZE, 0);  

                        if (recv_msg_lenth <= 0)    // length = 0 when client closed 
                        {
                            printf("Receive data NULL, client disconnected!\n");  

                            close(temp_connectfd);
                            FD_CLR(temp_connectfd, &readfds);   // close connect fd and clear in readfds set

                            for (int j = 0; j <= maxindex;  ++j) // if one connect fd closed, set arrry to -1
                            {
                                if (connectfd[j] == temp_connectfd)
                                    connectfd[j] = -1;
                            }

                            continue;
                        }

                        buf[recv_msg_lenth] = '\0';  
                        printf("Receive length:%d\nReceive data:%s\n", recv_msg_lenth, buf);  

                        if (send(temp_connectfd, buf, recv_msg_lenth, 0) == -1)  
                        {  
                            printf("Socket send echo failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
                            continue;
                        }  
                        send(temp_connectfd, "\r\n>", strlen("\r\n>"), 0);  
                    
                    }
                
                }

            }
        }

    }  

    close(listen_fd);  

    return 0;  
}  
