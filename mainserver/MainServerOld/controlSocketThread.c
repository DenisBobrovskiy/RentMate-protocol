//Command thread. Listening for user commands from LAN or remote connection
#include <stdio.h>
#include "controlSocketThread.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "generalSettings.h"
#include <fcntl.h>
#include "server.h"

#define RECVBUFLEN (MAXUSERCMDMAXLEN*3)

int yes = 1;
unsigned char processingBufCmd[MAXUSERCMDMAXLEN];


void *controlSockThread(void *args){

    int epoll_fd;
    struct epoll_event epollEvent, epollCurrentEvents[maxConcurentConnectionsToLANControlSocketPort];
    int eventCount;
    struct sockaddr_in newConnSockaddr;
    socklen_t newConnAddrLen;
    char newIpPresentation[INET_ADDRSTRLEN];
    unsigned char recvBuf[RECVBUFLEN];
    

    printf("CONTROL THREAD STARTED\n");

    //Listening socket using epoll() for listening for user commands from an app on a LAN or from the internet (NAT punchthrough)
    //This thread has 2 epoll() sockets =
    //1) For TLS NAT Punchtroughed connection over the internet (TLS because u can verify certificates as there is internet, certiifcate verification is done
    //on my own servers)
    //2) For LAN connection, first app listens for UDP brodcast messages, if there is one, give user an option to connect with his password (further hashed and
    //used as an encryption key)


    struct sockaddr_in LANControlSocketAddr;

    //Clear the addr before using
    memset(&LANControlSocketAddr,0,sizeof(struct sockaddr));

    int LANControlSocket = socket(AF_INET,SOCK_STREAM,0);
    //SOCKET()
    if(LANControlSocket==-1){
        perror("Faield to init socket in controlSocketThread.c\n");
        exit(1);
    }
    //REUSEADDR
    if(setsockopt(LANControlSocket,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))==-1){
        perror("Failed setting sockett as non-blocking\n");
        exit(1);
    }
    //BIND
    LANControlSocketAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    LANControlSocketAddr.sin_family = AF_INET;
    LANControlSocketAddr.sin_port = htons(LanControllSocketPort);
    if(bind(LANControlSocket,(struct sockaddr*)&LANControlSocketAddr,sizeof(LANControlSocketAddr))==-1){
        perror("Failed to bind socket in controlSocketThread.c\n");
        exit(1);
    }
    //LISTEN
    if(listen(LANControlSocket,maxConcurentConnectionsToLANControlSocketPort)==-1){
        perror("Failed to listen() in controlSocketThread.c\n");
        exit(1);
    }

    epoll_fd = epoll_create1(0);
    if(epoll_fd==-1){
        perror("Failed to set epoll_fd\n");
        exit(1);
    }
    epollEvent.events = EPOLLIN;
    epollEvent.data.fd = LANControlSocket;

    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,LANControlSocket,&epollEvent)==-1){
        perror("Failed to epoll_ctl add\n");
        exit(1);
    }

    //Main listening loop
    while(1){
        eventCount = epoll_wait(epoll_fd,epollCurrentEvents,maxConcurentConnectionsToLANControlSocketPort,-1);
        if(eventCount==-1){
            perror("Failed to epoll_wait()\n");
            exit(1);
        }
        for(int i = 0; i<eventCount; i++){
            printf("New epoll event in controlSocketThread.c\n");
            if(epollCurrentEvents[i].data.fd==LANControlSocket){
                //New connection
                newConnAddrLen = sizeof(newConnSockaddr);
                int newFd = accept(LANControlSocket,(struct sockaddr*)&newConnSockaddr,&newConnAddrLen);
                if(newFd==-1){
                    perror("Failed to accept()");
                    exit(1);
                }
                printf("New controll conn accepted\n");
                inet_ntop(AF_INET,&(newConnSockaddr.sin_addr),newIpPresentation,INET_ADDRSTRLEN);
                printf("Accepted new controlling connection at %s\n",newIpPresentation);
                if(fcntl(newFd,F_SETFL,O_NONBLOCK)==-1){
                    perror("Failed setting new conn as non blocking\n");
                    exit(1);
                }
                epollEvent.events = EPOLLIN;
                epollEvent.data.fd = newFd;
                if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,newFd,&epollEvent)==-1){
                    perror("Failed epoll_ctling\n");
                    exit(1);
                }
            }else{
                //New msg recieved
                recvAllCmdThread(epollEvent.data.fd,recvBuf,RECVBUFLEN,processingBufCmd,processMsgCallback);
            }
        }
    }

}

//PROCESSING CODE
int recvAllCmdThread(int socket, unsigned char *msgBuf, uint32_t msgBufLen, unsigned char *processBuf, int (*processessingBufCallback)(int)){
    printf("Recv all started\n");
    //Get msg len first
    unsigned char *msgBufPtr = msgBuf;
    unsigned char *currentMsgStartPtr = msgBuf;
    int currentBufferProgress = 0;
    uint16_t msgLen = 0;
    uint16_t msgLenRecieved = 0;
    char msgLenSpecifier = 0;
    int rs;
    do{
        printf("Looping recv loop\n");
        if((rs = recv(socket,msgBufPtr,msgBufLen-currentBufferProgress,0))==-1){
            //Error in recv()
            #if DEVTYPE==0
            perror("Recieving error");
            #elif DEVTYPE==1
            ESP_LOGI(TAG,"Recieving error\n");
            #endif
            exit(1);
        }
        printf("RS = %d\n",rs);
        if(rs==0){
            //Disconnected
            printf("Disconnected\n");
            close(socket);
            return 0;
        }else if(rs==-1){
            printf("Error in recv()\n");
            return 1;
        }

        for(int i = 0; i<msgBufLen; i++){
            printf("%d at %p ",*(msgBufPtr+i), msgBufPtr+i);
        }
        printf("\n");
        //Check if length recieved and if not recvieve it
        getMsgLen(&msgLen,&msgLenSpecifier,&currentMsgStartPtr,msgBufPtr,rs);
        //GOT LENGTH
        printf("Msg len: %d\n",msgLen);

        //If got length analyze message
        handleMsg(&msgLen,&msgLenRecieved,&msgLenSpecifier,&currentMsgStartPtr,msgBufPtr,msgBufLen,rs,processBuf,processessingBufCallback);

        //Looping buffer
        if(currentBufferProgress+rs<msgBufLen){
            //printf("Fits in buffer\n");
            //Enough space in buffer
            currentBufferProgress+=rs;
            msgBufPtr= msgBuf + currentBufferProgress;
        }else{
            //printf("Outside of buffer\n");
            //Loop buffer to initial pos
            currentBufferProgress = 0;
            msgBufPtr = msgBuf;
        }
    }while(rs>0);
    
}

void getMsgLen(uint16_t *msgLen, char *msgLenSpecifier,unsigned char **currentMsgStartPtr, unsigned char *msgBufPtr, int rs){
    if(*msgLen==0){
        if(*msgLenSpecifier!=1){
            printf("Msg start ptr assgined!\n");
            //Set msgPtr
            *currentMsgStartPtr = msgBufPtr;
        }
        //Get length if msgLen = 0 (means it is not set yet)
        //If recieved just 1 byte wait for another one cause length is 2 bytes
        if(rs==1){
            printf("Rs = 1\n");
            if(*msgLenSpecifier==1){
                //Got second half
                *msgLen = ntohs(*(uint16_t*)(msgBufPtr-1));
            }else{
                *msgLenSpecifier = 1;
            }
        }else if(rs>1){
            printf("Rs > 1\n");
            if(*msgLenSpecifier==1){
                //Got second half
                *msgLen = ntohs(*(uint16_t*)(msgBufPtr-rs));
            }else{
                //Got length on first try
                *msgLen = ntohs(*(uint16_t*)msgBufPtr);
                printf("Msg buf ptr: %d, %d, %d, %d, %d\n",*(msgBufPtr),*(msgBufPtr+1),*(msgBufPtr+2),*(msgBufPtr+3),*(msgBufPtr+4));
            }
        }
    }
}

void handleMsg(uint16_t *msgLen,
                uint16_t *msgLenRecieved, 
                char *msgLenSpecifier,
                unsigned char **currentMsgStartPtr, 
                unsigned char *msgBufPtr, 
                int msgBufLen, 
                int rs,
                unsigned char *processMsgBuf,
                int (*processessingBufCallback)(int))
{
    //If recieved length process the message
    if(*msgLen>0){
        *msgLenRecieved+=rs;
        printf("Msg len: %d/%d\n",*msgLenRecieved,*msgLen);
        if(*msgLenRecieved>=*msgLen){
            printf("COMPLETELY RECIEVED MSG. COPYING TO PROCESS BUFFER\n");
            //Got complete message
            //Check if its overlapping the buffer or not
            int difference = (*currentMsgStartPtr+*msgLen) - (msgBufPtr+msgBufLen);  //If negative or zero - not overlapping. If positive overlapping by this amount
            if(difference<=0){
                //Not overlapping
                printf("NOT OVERLAPING\n");
                memcpy(processMsgBuf, *currentMsgStartPtr,*msgLen);
                processessingBufCallback(*msgLen);
            }else{
                //Overlapping
                printf("OVERLAPPING\n");
                //Copy first part of msg
                memcpy(processMsgBuf,*currentMsgStartPtr,*msgLen-difference);
                //Copy second part
                memcpy(processMsgBuf+(*msgLen-difference),msgBufPtr,difference);
                processessingBufCallback(*msgLen);
            }  
        }else{
            printf("Msg incomplete, finishin it off\n");
        }
        //Check for parts of second msg
        if(*msgLenRecieved>=*msgLen){
            printf("Completely recieved first msg\n");
            //Some of second msg recieved or completely recieved first msg
            int newRs = *msgLenRecieved-*msgLen;
            //Some of second msg recieved
            if(*msgLenRecieved>*msgLen){
                msgBufPtr += *msgLen;
                *msgLen = 0;
                *msgLenSpecifier = 0;
                *msgLenRecieved = 0;
                printf("Some of second msg recieved\n");
                getMsgLen(msgLen,msgLenSpecifier,currentMsgStartPtr,msgBufPtr,newRs);
                printf("Second msg len %d\n",*msgLen);
                handleMsg(msgLen,msgLenRecieved,msgLenSpecifier,currentMsgStartPtr,msgBufPtr,msgBufLen,newRs,processMsgBuf,processessingBufCallback);
            }
            *msgLen = 0;
            *msgLenSpecifier = 0;
            *msgLenRecieved = 0;
        }
    }
}

int processMsgCallback(int msgLen){
    printf("Processing msg...\n");
    if(msgLen>MAXUSERCMDMAXLEN){
        printf("Msg too long. Ignoring it\n");
        return -1;
    }
    unsigned char settingsByte = *(processingBufCmd+2);
    commandRequestCommandControll_t newCmdReq;
    
    //Figure out general packet info and packet type (devCmd, powerOffCmd, etc)
    unsigned char gSettings = processingBufCmd[2];
    unsigned char pckType = processingBufCmd[3];
    int inx = 4;  //First index inside the underlying packet

    //Handle gSettings

    //Handle different packetTypes
    //DevCmdPacket
    if(pckType == 0){
        printf("Recieved a devCmd packet\n");
        unsigned char devId[DEVIDLEN];
        unsigned char devCmdSettings = processingBufCmd[inx];
        unsigned char *devIdPtr = processingBufCmd+inx+1;
        unsigned char *opCodePtr = devIdPtr+DEVIDLEN;
        unsigned char *isResOpCodePtr = opCodePtr+4;
        unsigned char *argsLenPtr = isResOpCodePtr+1;
        unsigned char *addLenPtr = argsLenPtr+4;
        unsigned char *argsPtr = addLenPtr+4;

        memcpy(devId,devIdPtr,DEVIDLEN);
        uint32_t opCode = ntohl(*(uint32_t*)opCodePtr);
        unsigned char isResOpCode = *isResOpCodePtr;
        uint32_t argsLen = ntohl(*(uint32_t*)argsLenPtr);
        uint32_t addLen = ntohl(*(uint32_t*)addLenPtr);

        unsigned char *addPtr = argsPtr+argsLen;

        //Add this command to cmdQueue for this devId
        addToCmdQueue(&commandsQueue,devIdPtr,opCode,devCmdSettings,argsPtr,argsLen,addPtr,addLen);

        
    }
}

