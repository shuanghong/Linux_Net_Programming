#include "Socket.h"

#include <cstring>

int main(int argc, char *argv[])  
{
    int listen_fd = 0;   
    int connect_fd = 0;  

    struct sockaddr_in server_addr;    
    struct sockaddr_in client_addr;  

    memset(&server_addr, 0, sizeof(server_addr));   
    memset(&client_addr, 0, sizeof(client_addr));  

    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port=htons(200);  

    Socket sock(Socket::create());
    //Socket sock(std::move(Socket::create()));

    sock.bind((struct sockaddr*)(&server_addr));
    sock.listen();

    std::cout << "Socket fd = " << sock.getFd() << std::endl;


    return 0;
}
