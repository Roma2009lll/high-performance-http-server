#ifndef SERVER_H
#define SERVER_H
#define PORT 9090
#define MAX_CONNECTIONS 50000
#define MAX_EVENTS 1024
#define BUF_SIZE 4028
#define TIMEOUT 30

#include <stdio.h>
#include <stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<ctype.h>
#include<time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>

typedef enum{
    STATE_READING,
    STATE_WRITING,
    STATE_CLOSED
}ConnectionState;

typedef struct{
    char method[16];
    char path[256];
    char http_version[16];
    int keep_alive;
    int content_lenght;
}REQUESET;

typedef struct Connection{
    int fd;
    char readBuf[8192];
    char writeBuf[8192];
    int readPos;
    ConnectionState state;
    int conState;
    int writePos;
    int writeEnd;
    time_t lastActivity;
    struct Connection* next;
    struct Connection* prew;
    REQUESET req;
}Connection;

typedef struct{
    char path[256];
    char* data;
    long size;
}CachedFile;

typedef void ACTION (Connection* connection,int epollFd);

extern int conectionCount;
extern int cacheCount;

void setNonBlocking(int fd);
int createServerSocket();
int setUpEpoll(int fd);
void eventLoop(int serverFd,int epollFd);
Connection* addConnection(int fd);
Connection* findConnection(int fd);
void deleteConnection(int fd,int epollFd);
void hadleReading(Connection* readConnection,int epollFd);
void handleWriting(Connection* writeConnectio,int epollFd);
ACTION* getAction(Connection* con);
void addToTail(Connection* con);
void removeFromTail(Connection* con);
void cheakTimeOuts(int epollFD);
int parseRequest(Connection* con);
void buildResponce(Connection* con);
CachedFile* getCachedFile(const char* path);

#endif