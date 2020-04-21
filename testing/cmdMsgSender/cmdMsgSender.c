#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <aes-gcm/aes-gcm.h>

#define PORT 23874
#define DEVIDLEN 16
#define KEYLEN 16
#define TAGLEN 16

int main(){
    //Generate cmdReq packet, encrypt and send over to server.c
    
    unsigned char newCmdMsg[255];
    mbedtls_gcm_context ctx;
    unsigned char gcmKey[KEYLEN] = "KeyKey123456";

    //Connecting
    unsigned char targetAddr[INET_ADDRSTRLEN] = "127.0.0.1";
    struct sockaddr_in connAddr;

    int localFd = socket(AF_INET,SOCK_STREAM,0);
    if(localFd==-1){
        perror("Failed starting socket");
        exit(1);
    }
    connAddr.sin_family = AF_INET;
    connAddr.sin_port = htons(PORT);
    connAddr.sin_addr.s_addr = inet_addr(targetAddr);
    if(connect(localFd,(struct sockaddr*)&connAddr,sizeof(connAddr))==-1){
        perror("Failed connecting");
        exit(1);
    }
    printf("Connected successfully.\n");


}

int generateCmdMsg(unsigned char *msgBuf,
                    unsigned char gSettings,
                    unsigned char pckType,
                    unsigned char devCmdSettings,
                    unsigned char *devId,
                    uint32_t opCode,
                    unsigned char isReservedOpCode,
                    uint32_t addLen,
                    unsigned char *add,
                    uint32_t argsLen,
                    unsigned char *args
                    )
{
    uint32_t msgTotalLen = 4 + 1 + DEVIDLEN + 4 + 1 + 4 + addLen + 4 + argsLen;
    printf("New msg len: %d\n",msgTotalLen);
    *(uint32_t*)msgBuf = msgTotalLen;
    unsigned char *gSettingsPtr = msgBuf+2;
    unsigned char *pckTypePtr = gSettingsPtr+1;
    unsigned char *devCmdSettingsPtr = pckTypePtr+1;
    unsigned char *devIdPtr = devCmdSettingsPtr+1;
    unsigned char *opCodePtr = devIdPtr+DEVIDLEN;
    unsigned char *isReservedOpCodePtr = opCodePtr+4;
    unsigned char *addLenPtr = isReservedOpCodePtr+1;
    unsigned char *addPtr = addLenPtr+4;
    unsigned char *argsLenPtr = addPtr+addLen;
    unsigned char *argsPtr = argsLenPtr+4;

    *gSettingsPtr = gSettings;
    *pckTypePtr = pckType;
    *devCmdSettingsPtr = devCmdSettings;
    memcpy(devIdPtr, devId, DEVIDLEN);
    *(uint32_t*)opCodePtr = opCode;
    *isReservedOpCodePtr = isReservedOpCode;
    *(uint32_t*)addLenPtr = addLen;
    memcpy(addLenPtr,add,addLen);
    *(uint32_t*)argsLenPtr = argsLen;
    memcpy(argsPtr,args,argsLen);
}