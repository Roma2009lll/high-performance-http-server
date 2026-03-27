#include "../include/server.h"

int conectionCount=0;
int cacheCount = 0;

int main(void){
    struct epoll_event* event;
    int server=createServerSocket();
    int epoll=setUpEpoll(server);
    eventLoop(server,epoll);
    return 0;
    //gcc -O3 src/server.c src/main.c -o my_server
    //./my_server
    // ab -k -c 20000 -n 500000 http://127.0.0.1:9090/
}