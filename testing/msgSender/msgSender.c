#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <stdlib.h>
#include "aes-gcm/aes-gcm.h"
#include "msgSender.h"
#include "time.h"
#include "pckData/pckData.h"

#define MAXMSGLEN 255
#define RECVBUFLEN 150
#define DEVTYPE 0    //CAN BE "LINUX"(0) or "ESP32"(1). Changes functions accordingly to platforms

//IMPORTANT DEFINES
#define DEVIDLEN 12
#define IVLEN 12
#define TAGLEN 16
#define NONCELEN 4
#define KEYLEN 16

//NETWORK DATA
char targetIp[INET_ADDRSTRLEN] = "127.0.0.1";
uint16_t port = 3333;

//PACKET DATA
unsigned char tempDevId[12] = "DevId1234567";
unsigned char tempIv[12] = "poiuytrewqpo";
unsigned char *tempAdd = NULL;
int currentMsgLen = 0;

//ENCRYPTED DATA
unsigned char gcmKey[KEYLEN] = "KeyKeyKey1234567";
mbedtls_gcm_context ctx;
unsigned char msgBuf[MAXMSGLEN];
unsigned char msgBuf2[MAXMSGLEN];
uint32_t opCode = 0;
unsigned char args[20] = "Oiiii mate!";

//RECV ALL
unsigned char recvBuf[RECVBUFLEN];
unsigned char processBuf[MAXMSGLEN];


int main(void){
    unsigned char *pckData;
    unsigned char *addPckData;
    unsigned char newMsg[20] = "1234567890poiuyt1234";
    unsigned char newMsg2[6] = "oioioi";
    unsigned char newAdd1[12] = "DevId1234567";
    unsigned char newAdd2[8] = "09876543";
    unsigned char outBuf[124];
    initGCM(&ctx,gcmKey,KEYLEN*8);
    //Normal data
    initPckData(&pckData);
    appendToPckData(&pckData,newMsg,20);
    appendToPckData(&pckData,newMsg,6);
    print2("pckData:",pckData,8+24+10,0);
    //Add data
    initPckData(&addPckData);
    appendToPckData(&addPckData,newAdd1,12);
    appendToPckData(&addPckData,newAdd2,8);
    uint32_t msgLen = encryptPckData(&ctx, pckData,addPckData,NULL,0,outBuf);

    if(msgLen==-1){
        printf("Failed encrypting data\n");
        exit(1);
    }
    
    int localSock = socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in newConnAddr;
    newConnAddr.sin_addr.s_addr = inet_addr(targetIp);
    newConnAddr.sin_family = AF_INET;
    newConnAddr.sin_port = htons(port);
    socklen_t sLen = sizeof(newConnAddr);
    if(connect(localSock,(struct sockaddr*)&newConnAddr,sLen)==-1){
        perror("Fail connect");
        exit(1);
    }
    printf("Connected! Sending msg of len: %d\n",msgLen);
    sendall(localSock,outBuf,msgLen);

    /*
    //MSG NUM 1
    unsigned char generalSettings;
    initGeneralSettings(&generalSettings,0,1);
    composeMsg(&ctx,msgBuf,tempDevId,tempIv,tempAdd,0,848,generalSettings,0,args,20);

    //MSG NUM 2
    unsigned char generalSettings2;
    initGeneralSettings(&generalSettings2,0,1);
    composeMsg(&ctx,msgBuf2,tempDevId,tempIv,tempAdd,0,849,generalSettings2,0,args,20);

    struct sockaddr_in target;
    printf("Sending msg...\n");

    //Get data for socket and create it
    int s = socket(AF_INET,SOCK_STREAM,0);
    if(s == -1){
        perror("Failed to init socket in msgSender.c\n");
        exit(1);
    }
    target.sin_family = AF_INET;
    target.sin_port = htons(port);
    target.sin_addr.s_addr = inet_addr(targetIp);

    if(connect(s, (struct sockaddr*)&target,sizeof(target))==-1){
        perror("Connect failed");
        exit(1);
    }
    printf("Connected!\n");

    int st;
    if((st = send(s,msgBuf,currentMsgLen,0))==-1){
        perror("Failed to send");
    }
    //xsleep_ms(500);
    if((st = send(s,msgBuf2,currentMsgLen,0))==-1){
        perror("Failed to send");
    }
    printf("Data sent %i\n",st);

    //printf("Recieving response\n");
    //recvAll(s,msgBuf,RECVBUFLEN,processBuf,processProcessBuffer);

    //Entering main loop
    while(1){
        //BEACON MSG
        sleep_ms(2000);
        unsigned char generalSettingsBeacon;
        initGeneralSettings(&generalSettingsBeacon,1,1);
        composeMsg(&ctx,msgBuf2,tempDevId,tempIv,tempAdd,0,850,generalSettingsBeacon,1,args,20);
        int st;
        if((st = send(s,msgBuf2,currentMsgLen,0))==-1){
            perror("Failed to send");
        }           
        printf("Sending beacon\n");
    }
*/
}

void print3(unsigned char *data, int len){
    for(int i =0; i<len;i++){
        printf("%d ",*(data+i));
    }
    printf("\n");
}

int composeMsg(mbedtls_gcm_context *ctx,
                unsigned char *msgBuf, 
                unsigned char *devId, 
                unsigned char *iv, 
                unsigned char *add, 
                uint32_t addLen,
                uint32_t nonce, 
                unsigned char generalSettings,
                uint32_t opCode,
                unsigned char *commandsArgs,
                uint32_t commandArgsLen)
                
{
    //ASSIGN MSG DATA
    unsigned char *devIdPtr = msgBuf+3;
    unsigned char *ivPtr = devIdPtr+DEVIDLEN;
    unsigned char *tagPtr = ivPtr+IVLEN;
    unsigned char *addPtr = tagPtr+TAGLEN;
    unsigned char *noncePtr = addPtr+addLen;
    unsigned char *generalSettingsPtr = noncePtr+NONCELEN;
    unsigned char *opCodePtr = generalSettingsPtr+1;
    unsigned char *argsLenPtr = opCodePtr+4;
    unsigned char *argsPtr = argsLenPtr+4;


    //PLAINTEXT PART
    //Length
    uint16_t len = 4+4+DEVIDLEN+IVLEN+TAGLEN+addLen+NONCELEN+1+4+commandArgsLen;
    currentMsgLen = len;
    printf("Length: %d\n",len);
    *(uint16_t*)msgBuf = htons(len);
    //AddLen
    msgBuf[2] = addLen;
    //DevId
    memcpy(devIdPtr,devId,DEVIDLEN);
    //Add
    memcpy(addPtr,add,addLen);


    //ENCRYPTED PART
    int edLen = NONCELEN+1+4+commandArgsLen;
    //Nonce
    *(uint32_t*)noncePtr = htonl(nonce);
    //General settings
    *generalSettingsPtr = generalSettings;
    //OpCode
    *(uint32_t*)opCodePtr = htonl(opCode);
    //Args len
    *(uint32_t*)argsLenPtr = htonl(commandArgsLen);
    //Command args
    memcpy(argsPtr,commandsArgs,commandArgsLen);

    for(int i = 0; i<len;i++){
        printf("%i ",*(msgBuf+i));
    }
    printf("\n");

    //Encrypt this data right inside the msgBuffer copying the results above themselves
    encryptGcm(ctx,noncePtr,edLen,ivPtr,IVLEN,add,addLen,noncePtr,tagPtr,TAGLEN);
}

void initGeneralSettings(unsigned char *gSettings, unsigned char isTerminator, unsigned char isOpCodeReserved){
    *gSettings = 0;

    *gSettings |= (isTerminator<<7); //Set 0th bit to isTerminator values (0 or 1)
    *gSettings |= (isOpCodeReserved<<6); //Set 1st bit to isOpCodeReserved values (0 or 1)
}

//Blocks until recieved terminator msg (terminator bit set in gSettings byte in msg) or timeout happens
/*int recvAll(int socket, unsigned char *msgBuf, uint32_t msgBufLen, unsigned char *processBuf, int (*processessingBufCallback)(int)){
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
}*/

int processProcessBuffer(int msgLen){
    printf("Processing callback\n");
    printf("Priting recieved msg in prcoess buf with length: %d\n",msgLen);
    for(int i = 0; i<msgLen;i++){
        printf("%d ",*(processBuf+i));
    }
    printf("\n");

    unsigned char addLen = *(processBuf+2);
    unsigned char *ivPtr = processBuf+3;
    unsigned char *tagPtr = ivPtr+IVLEN;
    unsigned char *addPtr;
    printf("IV:\n");
    print3(ivPtr,IVLEN);
    printf("TAG:\n");
    print3(tagPtr,TAGLEN);
    if(addLen>0){
        addPtr = tagPtr+TAGLEN;
    }else{
        addPtr = NULL;
    }
    //Now decrypt to proceed
    int eDataLen = msgLen-3-IVLEN-TAGLEN-addLen;
    printf("eDataLen: %d\n",eDataLen);
    printf("Ciphertext:\n");
    print3(tagPtr+TAGLEN+addLen,eDataLen);
    unsigned char decryptOut[eDataLen];
    printf("Decrypting...\n");
    if(decryptGcm(&ctx,tagPtr+TAGLEN+addLen,eDataLen,ivPtr,IVLEN,addPtr,addLen,tagPtr,TAGLEN,decryptOut)!=0){
        printf("Failed to decrypt\n");
        return -1;
    }
    printf("Decrypted data\n");
    uint32_t nonce = ntohl(*(uint32_t*)decryptOut);
    unsigned char gSettings = *(decryptOut+NONCELEN);
    uint32_t opCode = ntohl(*(uint32_t*)(decryptOut+NONCELEN+1));
    uint32_t argsLen = ntohl(*(uint32_t*)(decryptOut+NONCELEN+1+4));
    unsigned char *argsPtr = decryptOut+NONCELEN+1+4+4;
    printf("Recieved: Nonce: %d, gSettings: %d, opCode: %d, argsLen: %d\n",nonce,gSettings,opCode,argsLen);
}

void sleep_ms(int millis){
    //printf("Sleeping for %i ms\n",millis);
    struct timespec ts;
    ts.tv_sec = millis/1000;
    ts.tv_nsec = (millis%1000) * 1000000;
    nanosleep(&ts, NULL);
}
