#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

const int STDIN = 0; // fd for standard input
const int8_t SIZE = 100;

int main(void)
{
    char inputData[SIZE];

    fd_set readfds;

    FD_ZERO(&readfds);   // clear readfds bits
    FD_SET(STDIN, &readfds);    // enable bit in readfds
    
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 500000;    // 10.5s timeout

    select(STDIN + 1, &readfds, nullptr, nullptr, &timeout);    //wait for read event, write/exception are ignore

    if (FD_ISSET(STDIN, &readfds))  // is the bit set in readfds
    {
        fgets(inputData, SIZE, stdin);
        printf("Data input: %s\n", inputData);
    }
    else
        printf("Time out!\n");
        
    return 0;
}
