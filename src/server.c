#include "../include/server.h"

CachedFile cache[100];
Connection allConections[MAX_CONNECTIONS];
Connection* head=NULL;
Connection* tail=NULL;

void setNonBlocking(int fd){
    int flags=fcntl(fd,F_GETFL,0);
    if(flags==-1){
        perror("Error in getting flags");
        return;
    }
    if((fcntl(fd,F_SETFL,flags|O_NONBLOCK))==-1){
        perror("Error in setting new flags\n");
    }
}

int createServerSocket(){
    int server=socket(AF_INET,SOCK_STREAM,0);
    if(server==-1){
        perror("Error in creating socket");
        return -1;
    }
    int opt = 1;
    if((setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) == -1){
        perror("Error in setsockpt");
        return -1;
    }
    setNonBlocking(server);
    struct sockaddr_in serverAddr={
        .sin_family=AF_INET,
        .sin_port=htons(PORT),
        .sin_addr.s_addr=INADDR_ANY
    };
    int resBind=bind(server,(struct sockaddr*)&serverAddr,sizeof(serverAddr));
    if(resBind==-1){
        perror("Error in reservating port");
        return -1;
    }
    if(( listen(server,SOMAXCONN))==-1){
    perror("Listening failed\n");
    return -1;
    }
    return server;
}

int setUpEpoll(int fd){
    int epollFd=epoll_create1(0);
    if(epollFd==-1){
        perror("Error in creating epoll");
        return -1;
    }
    struct epoll_event event={
        .events=EPOLLIN,
        .data.fd=fd
    };
    if((epoll_ctl(epollFd,EPOLL_CTL_ADD,fd,&event))==-1){
        perror("Error in epoll_ctl");
        return -1;
    }
    return epollFd;
}

void eventLoop(int serverFd,int epollFd){
    while(true){
        struct epoll_event events[MAX_EVENTS];
        int n=epoll_wait(epollFd,events,MAX_EVENTS,1000);
        if(n==-1){
            perror("epoll_wait");
            break;
        }
        for(int i=0;i<n;i++){
            int tempFd=events[i].data.fd;
            if(tempFd==serverFd){
                printf("Thera are new connection\n");
                while(1){
                    int client=accept(serverFd,NULL,NULL);
                    if (client == -1) {
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            break;
                        }else{
                            perror("Accept error");
                            break;
                        }
                    }
                    setNonBlocking(client);
                    Connection* newConnection=addConnection(client);
                    if(newConnection==NULL){
                        perror("Max connection reached\n");
                        continue;
                    }
                    struct epoll_event clinetEvent={
                        .events=EPOLLIN|EPOLLET,
                        .data.ptr=newConnection
                    };
                    if((epoll_ctl(epollFd,EPOLL_CTL_ADD,newConnection->fd,&clinetEvent))==-1){
                        perror("Connection failed\n");
                        deleteConnection(client,epollFd);
                    }
                    printf("Client %d connected\n",client);
                }
            }else{
                Connection* cur=(Connection*)events[i].data.ptr;
                ACTION* res= getAction(cur);
                if(res!=NULL){
                    res(cur,epollFd);
                }
            }
        }
        cheakTimeOuts(epollFd);
    }
}

Connection* addConnection(int fd){
    if(fd<0||conectionCount>=MAX_CONNECTIONS){
        return NULL;
    }
    Connection* newConnection=&allConections[fd];
    newConnection->fd=fd;
    newConnection->state=STATE_READING;
    newConnection->lastActivity=time(NULL);
    newConnection->readPos=0;
    newConnection->writePos=0;
    newConnection->writeEnd=0;
    newConnection->conState=0;
    conectionCount++;
    addToTail(newConnection);
    return newConnection;
}

Connection* findConnection(int fd){
    if(fd<0){
        return NULL;
    }
    Connection* temp=&allConections[fd];
    if(temp->state==STATE_CLOSED){
        return NULL;
    }
    return temp;
}

void deleteConnection(int fd,int epollFd){
    if(fd<0){
        return;
    }
    if((epoll_ctl(epollFd,EPOLL_CTL_DEL,fd,NULL))==-1){
        perror("Deletion failed\n");
        return;
    }
    allConections[fd].state=STATE_CLOSED;
    allConections[fd].fd = -1;
    allConections[fd].readPos = 0;
    removeFromTail(&allConections[fd]);
    conectionCount--;
    close(fd);
}

void hadleReading(Connection* readConnection,int epollFd){
    int leftSpace=sizeof(readConnection->readBuf)-readConnection->readPos-1;
    if(leftSpace<=0){
        perror("No room for reading");
        return;
    }
    while(true){
        int n=read(readConnection->fd,readConnection->readBuf+readConnection->readPos,leftSpace);
        if(n>0){
            readConnection->readPos+=n;
            readConnection->readBuf[readConnection->readPos]='\0';
            removeFromTail(readConnection);
            addToTail(readConnection);
            readConnection->lastActivity=time(NULL);
            if(parseRequest(readConnection)==1){
                buildResponce(readConnection);
                readConnection->writePos=0;
                readConnection->state=STATE_WRITING;
                readConnection->conState=1;
                struct epoll_event ev={
                    .events=EPOLLOUT|EPOLLET,
                    .data.ptr=readConnection
                };
                if((epoll_ctl(epollFd,EPOLL_CTL_MOD,readConnection->fd,&ev))==-1){
                    perror("Error in epoll_ctl");
                    return;
                }
                break;
            }
        }else if(n==0){
            deleteConnection(readConnection->fd,epollFd);
            break;
        } else{
            if(errno == EAGAIN || errno == EWOULDBLOCK){
             break;
            }else{
                deleteConnection(readConnection->fd,epollFd);
            }
        }
    }
}

void handleWriting(Connection* writeConnection,int epollFd){
    if(writeConnection->state==STATE_WRITING){
        while(true){
            int leftToSend=writeConnection->writeEnd-writeConnection->writePos;
            int n=send(writeConnection->fd,writeConnection->writeBuf+writeConnection->writePos,leftToSend,0);
            if(n>0){
                writeConnection->writePos += n;
                removeFromTail(writeConnection);
                addToTail(writeConnection);
                writeConnection->lastActivity = time(NULL);
                if (writeConnection->writePos == writeConnection->writeEnd) {
                    if (writeConnection->req.keep_alive == 1) {
                        writeConnection->writePos = 0;
                        writeConnection->writeEnd = 0;
                        writeConnection->readPos = 0;
                        writeConnection->state = STATE_READING;
                        writeConnection->conState = 0;
                        struct epoll_event ev = {
                            .data.ptr = writeConnection,
                            .events = EPOLLIN
                        };
                        if ((epoll_ctl(epollFd, EPOLL_CTL_MOD, writeConnection->fd, &ev)) == -1) {
                            perror("Epoll_ctl failed\n");
                            return;
                        }
                        break;
                    } else {
                        deleteConnection(writeConnection->fd, epollFd);
                        break;
                    }
                }
           }else if(n==-1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }else{
                perror("Send error\n");
                deleteConnection(writeConnection->fd, epollFd);
                break;
            }
           }
        }
    }
}

ACTION* getAction(Connection* con){
    static ACTION* arr[]={hadleReading,handleWriting};
    return arr[con->conState];
}

void addToTail(Connection* con){
    if(head==NULL&&tail==NULL){
        head=con;
        tail=con;
        con->next=NULL;
        con->prew=NULL;
    }else{
        con->prew=tail;
        tail->next=con;
        tail=con;
        con->next=NULL;
    }
}

void removeFromTail(Connection* con){
    if(con==NULL){
        return;
    }
    if(con->prew!=NULL){
        con->prew->next=con->next;
    }else{
        head=con->next;
    }
    if(con->next!=NULL){
        con->next->prew=con->prew;
    }else{
        tail=con->prew;
    }
    con->next=NULL;
    con->prew=NULL;
}

void cheakTimeOuts(int epollFd){
    time_t now=time(NULL);
    while(head!=NULL){
        if((now-head->lastActivity)>TIMEOUT){
            printf("Client on FD %d timed out\n",head->fd);
            deleteConnection(head->fd,epollFd);
        }else{
            break;
        }
    }
}

int parseRequest(Connection* con){
    char* headerEnd=strstr(con->readBuf,"\r\n\r\n");
    if(headerEnd==NULL){
        return 0;
    }
    memset(&con->req, 0, sizeof(REQUESET));
    sscanf(con->readBuf, "%15s %255s %15s",con->req.method,con->req.path,con->req.http_version);
    char* currentLine=strstr(con->readBuf,"\r\n");
    if(currentLine!=NULL){
        currentLine+=2;
    }
    while(currentLine!=NULL&&currentLine<headerEnd){
        if (strncasecmp(currentLine, "Connection: keep-alive", 22) == 0) {
            con->req.keep_alive = 1;
        }
        else if (strncasecmp(currentLine, "Content-Length:", 15) == 0) {
            con->req.content_lenght = atoi(currentLine + 15);
        }
        currentLine=strstr(currentLine,"\r\n");
        if(currentLine!=NULL){
            currentLine+=2;
        }
    }
    return 1;
}

void buildResponce(Connection* con){
    if(strcmp(con->req.method,"GET")!=0){
        const char* err405 = "HTTP/1.1 405 Method Not Allowed\r\nConnection: close\r\n\r\n";
        strcpy(con->writeBuf, err405);
        con->writeEnd = strlen(err405);
        con->req.keep_alive = 0;
        return;
    }
    if (strstr(con->req.path, "..") != NULL) {
        const char* err403 = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
        strcpy(con->writeBuf, err403);
        con->writeEnd = strlen(err403);
        con->req.keep_alive = 0;
        return;
    }
    char filePath[512];
    if(strcmp(con->req.path,"/")==0){
        strcpy(filePath,"./index.html");
    }else{
       snprintf(filePath, sizeof(filePath), ".%s", con->req.path);
    }
    CachedFile* fp1=getCachedFile(filePath);
    memcpy(con->writeBuf, fp1->data, fp1->size);
    con->writeEnd = fp1->size;
}

CachedFile* getCachedFile(const char* path){
    for(int i=0;i<cacheCount;i++){
        if(strcmp(cache[i].path,path)==0){
            return &cache[i];
        }
    }
    if (cacheCount >= 100) {
        static CachedFile error500 = {
            .data = "HTTP/1.1 500 Internal Server Error\r\n\r\nCache is full",
            .size = 51
        };
        strcpy(error500.path, "500_cache");
        return &error500;
    }
    FILE* fp=fopen(path,"rb");
    if(fp==NULL){
        static CachedFile error={
            .data="HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found",
            .size=62
        };
        strcpy(error.path,"404");
        return &error;
    }
    fseek(fp,0,SEEK_END);
    long fileSize=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(fileSize+1024 > 8192){
        fclose(fp);
        static CachedFile error={
            .data="HTTP/1.1 500 Internal Server Error\r\n\r\nFile too large",
            .size=53
        };
        strcpy(error.path,"500");
        return &error;
    }
    int totalSize=1024+fileSize;
    cache[cacheCount].data=(char*)malloc(totalSize);
    strcpy(cache[cacheCount].path,path);
    int headerLen = sprintf(cache[cacheCount].data,
         "HTTP/1.1 200 OK\r\n"
         "Content-Length: %ld\r\n"
         "Connection: keep-alive\r\n"
         "\r\n",
         fileSize
    );
    fread(cache[cacheCount].data+headerLen,fileSize,1,fp);
    fclose(fp);
    cache[cacheCount].size=fileSize+headerLen;
    cacheCount++;
    return &cache[cacheCount-1];
}