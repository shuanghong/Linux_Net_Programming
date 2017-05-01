
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024 
const static char *welcome_msg = "Welcome to socket server";

void * SocketHandle(void* connect_fd)
{
    char buf[BUFSIZE] = {0};    
    int recv_msg_lenth = 0;  

    int temp_connect_sockfd = *((int *)connect_fd);  
      
    printf("ThreadID:%ld, New socket handle!\n", pthread_self());  

    while(1) 
    {
        send(temp_connect_sockfd, "\r\n>", strlen("\r\n>"), 0);  

        recv_msg_lenth = recv(temp_connect_sockfd, buf, BUFSIZE, 0);  

        if (recv_msg_lenth <= 0)    // length = 0 when client closed and jump out of the loop
        {
            printf("ThreadID:%ld, receive data NULL, client disconnected!\n", pthread_self());  
            break;
        }

        buf[recv_msg_lenth] = '\0';  
        printf("ThreadID:%ld, Receive length:%d\nReceive data:%s\n", pthread_self(), recv_msg_lenth, buf);  

        if (send(temp_connect_sockfd, buf, recv_msg_lenth, 0) == -1)  
        {  
            printf("ThreadID:%ld, Socket send failed, error_num=%d, error_str=%s!\n", pthread_self(), errno, strerror(errno));  
            break;
        }  
    }

    close(temp_connect_sockfd);  // close connect fd when current client closed
    pthread_exit(NULL);
}


int main(int argc, char *argv[])  
{
    pthread_t thread_id = 0u;          // identifer for new thread
    int ret = 0;

    int listen_fd = 0;   
    int connect_fd = 0;  
    socklen_t sin_size = sizeof(struct sockaddr_in);  

    struct sockaddr_in server_addr;    
    struct sockaddr_in client_addr;  

    memset(&server_addr, 0, sizeof(server_addr));   
    memset(&client_addr, 0, sizeof(client_addr));  

    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port=htons(200);  
      
    if ((listen_fd = socket(PF_INET,SOCK_STREAM,0)) == -1)  
    {    
        printf("Socket create failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        exit(1);  
    }  
   
    int reuse_addr_opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt)) == -1)
    {
        printf("Socket set reuse addr option failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        exit(1);  
    }

    int reuse_port_opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &reuse_port_opt, sizeof(reuse_port_opt)) == -1)
    {
        printf("Socket set resue port option failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        exit(1);  
    }

    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)  
    {  
        printf("Socket bind failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        exit(1);  
    }  
      
    if (listen(listen_fd, 5) == -1)
    {
        printf("Socket listen failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        exit(1);  
    } 

    while(1)
    {
        connect_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &sin_size);  
        if (connect_fd == -1)
        {  
            printf("Socket accept failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
            continue;
        }  
        printf("Client connected, addr:%s\n", inet_ntoa(client_addr.sin_addr));  

        send(connect_fd, welcome_msg, strlen(welcome_msg), 0);  

        ret = pthread_create(&thread_id, NULL, SocketHandle, &connect_fd);
        if (ret != 0)
        {
            printf("%s(): thread create failed\n", __PRETTY_FUNCTION__);
            break;
        }

    }
    close(listen_fd);  
    pthread_join(thread_id, NULL);    // block self and wait for thread stop(call pthread_exit() )

    return 0;
}
