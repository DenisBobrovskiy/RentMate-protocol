#include "testPckData.h"
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include "../pckData.h"
#include <unistd.h>
#include <fcntl.h>

#define KEYLEN 16

unsigned char processingBuf[255];

int main(void){
    printf("Started\n");

    arrayList recvHolders;
    arrayList connInfos;
    arrayList devIDs;
    arrayList commandsQueue;
    globalSettingsStruct globalSettings;

    initBasicServerData(&connInfos,&recvHolders,&devIDs,&commandsQueue,&globalSettings,0);
    mbedtls_gcm_context ctx;
    mbedtls_gcm_context ctx2;
    unsigned char gcmKey[KEYLEN] = "KeyKeyKey123456";
    initGCM(&ctx,gcmKey,KEYLEN*8);
    initGCM(&ctx2,gcmKey,KEYLEN*8);
    unsigned char *pckData;
    if(initPckData(&pckData)!=0){
        perror("Failed to initPckData");
        exit(1);
         
    }
    unsigned char newData[16] = "0987654321poqwut";
    if(appendToPckData(&pckData,newData,16)!=0){
        perror("Failed to append to pckData");
        exit(1);
    }
    uint32_t pckGSettings = 0;
    unsigned char pckBuf[255];
    unsigned char outPckBuf[255];
    uint32_t newNonce = 536636;
    printf("FFFFFFFFFFFFFFFFFFFFFF\n");
    encryptPckData(&ctx,pckData,NULL,NULL,pckGSettings,newNonce,pckBuf);
    printf("FFFFFFFFFFFFFFFFFFFFFF\n");
    print2("PCKBUFFF:",pckBuf,255,0);
    decryptPckData(&ctx,pckBuf,outPckBuf);
    printf("Initializing socket\n");
    //Testing data recieving
    struct epoll_event event, events[10];
    int yes = 1;
    struct sockaddr_in localSockAddr;
    int localSock = socket(AF_INET,SOCK_STREAM,0);
    if(localSock==-1){
        perror("failed making socket\n");
    }
    if(setsockopt(localSock,SOL_SOCKET,SO_REUSEADDR, &yes,sizeof(yes))==-1){
        perror("setsockopt");
    }
    if(fcntl(localSock,F_SETFL,O_NONBLOCK)==-1){
        perror("fcntl\n");
    }
    localSockAddr.sin_family = AF_INET;
    localSockAddr.sin_addr.s_addr = INADDR_ANY;
    localSockAddr.sin_port = htons(1134);
    if(bind(localSock,(struct sockaddr*)&localSockAddr,sizeof(localSockAddr))==-1){
        perror("bind");
    }
    if(listen(localSock,10)==-1){
        perror("listen\n");
    }

    int epoll_fd = epoll_create1(0);
    event.data.fd = localSock;
    event.events = EPOLLIN;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,localSock,&event);


    while(1){
        printf("Looping\n");
        int eventCount;
        eventCount= epoll_wait(epoll_fd,events,10,-1);
        for(int i =0; i<eventCount; i++){
            printf("Processing epoll entry\n");
            if(events[i].data.fd==localSock){
                printf("New connection\n");
                struct sockaddr_in newConnAddr;
                socklen_t len = (socklen_t)sizeof(newConnAddr);
                int newSock = accept(localSock,(struct sockaddr*)&newConnAddr, &len);
                char newIp[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET,&(newConnAddr.sin_addr),newIp,INET6_ADDRSTRLEN);
                printf("New conn ip: %s\n",newIp);
                event.data.fd = newSock;
                event.events = EPOLLIN;
                epoll_ctl(epoll_fd,EPOLL_CTL_ADD,newSock,&event);
            }else if(events[i].data.fd!=localSock){
                printf("Data recieved\n");
                recvAll(&recvHolders, &connInfos, event.data.fd,processingBuf,processNewMsg);
            }
        }
    }
}

int processNewMsg(){
    printf("Processing msg\n");
    mbedtls_gcm_context ctx;
    unsigned char out[255];
    initGCM(&ctx,"KeyKeyKey1234567",KEYLEN*8);
    uint32_t msgLen = ntohl(*(uint32_t*)processingBuf);
    uint32_t addLen = ntohl(*(uint32_t*)(processingBuf+4));
    uint32_t extraDataLen = ntohl(*(uint32_t*)(processingBuf+8));
    printf("Msg len: %d, Add len: %d\n",msgLen,addLen);
    decryptPckData(&ctx,processingBuf,out);
    arrayList pckDataPointers;
    readDecryptedData(out,&pckDataPointers);

    //ADD
    arrayList addDataPtrs;
    readAddData(processingBuf,&addDataPtrs);
    unsigned char *devIdPtr = NULL;
    uint32_t currentElLen = getElementFromPckData(&addDataPtrs,&devIdPtr,0);
    printf("Add data len: %d\n",currentElLen);
    //print2("DevId(add):",devIdPtr,12,1);

    //Encrypted data
    unsigned char *dataPtr;
    int dataLen = 0;
    dataLen = getElementFromPckData(&pckDataPointers,&dataPtr,0);
    printf("Data len: %d\n",dataLen);
    print2("First pckData element:",dataPtr,dataLen,0);

    printf("MEMORY ANALYSIS\n");
    printf("decrypted data:\n");
    for(int i = 0; i<255;i++){
        printf("%d |Location: %p, Data: %d|\n  ",i,processingBuf+i,*(processingBuf+i));
    }
    printf("Data ptr: %p\n",devIdPtr);
    //print2("Data ptr (int):",&dataPtr,8,0);
    print2("Data",devIdPtr,currentElLen,1);
    dataLen = getElementFromPckData(&pckDataPointers,&dataPtr,1);
    print2("Data2:",dataPtr,dataLen,0);
}
