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

int main(int argc, char *argv[])  
{  
    char buf[BUFSIZE] = {0};    
    int recv_msg_lenth = 0;  
    int listen_fd = 0;   
    int connect_fd = 0;  
    socklen_t sin_size = 0u;  

    struct sockaddr_in server_addr;    
    struct sockaddr_in client_addr;  

    memset(&server_addr, 0, sizeof(server_addr));   
    memset(&client_addr, 0, sizeof(client_addr));  

    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port=htons(200);  
      
    if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)  
    {    
        printf("Socket create failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
        exit(1);  
    }  

    int reuse_addr_opt = 1;
    if ( setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr_opt, sizeof(reuse_addr_opt)) == -1 )
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
      
    sin_size = sizeof(struct sockaddr_in);  
      
    while(1)    // loop for handle next client connection if current closed 
    {
        connect_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &sin_size);  
        if (connect_fd == -1)
        {  
            printf("Socket accept failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
            continue;
        }  
        printf("Client connected, addr:%s\n", inet_ntoa(client_addr.sin_addr));  

        send(connect_fd, welcome_msg, strlen(welcome_msg), 0);  

        while(1)    // loop for handle current client connection for multiple messages
        {
            send(connect_fd, "\r\n>", strlen("\r\n>"), 0);  

            recv_msg_lenth = recv(connect_fd, buf, BUFSIZE, 0);  

            if (recv_msg_lenth <= 0)    // length = 0 when client closed and jump out of the loop
            {
                printf("Receive data NULL, client disconnected!\n");  
                break;
            }

            buf[recv_msg_lenth] = '\0';  
            printf("Receive length:%d\nReceive data:%s\n", recv_msg_lenth, buf);  

            if (send(connect_fd, buf, recv_msg_lenth, 0) == -1)  
            {  
                printf("Socket send failed, error_num=%d, error_str=%s!\n", errno, strerror(errno));  
                break;
            }  
        }

        close(connect_fd);  // close connect fd when current client closed
    }  

    close(listen_fd);  

    return 0;  
}  
