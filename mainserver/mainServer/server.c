#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/random.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

//CUSTOM
#include "commands.h"
#include "mbedtls/include/mbedtls/gcm.h"
#include "ArrayList/arrayList.h"
#include "AES-GCM/aes-gcm.h"
#include "pckData/pckData.h"
#include "pckData/generalSettings.h"
#include "server.h"
#include "epollRecievingThread.h"

#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_BLUE  "\x1B[34m"
#define ANSI_COLOR_RESET "\x1b[0m"


//MULTITHREADING
pthread_t epollRecievingThreadID; //Thread that listens for node and user commands


arrayList connectionsInfo;  //Store info for every connection to server
arrayList recvHolders;  //Necesarry for storing recieved data sent in multiple packets. Used in a more advanced implementation of recv() function.
arrayList devInfos; //Stores information for every device known to the server
arrayList commandsQueue;    //Stores the commands recieved from a client(used not node). Queued up for execution
globalSettingsStruct globalSettings;    //global settings. Loaded and saved in a config plaintext file

unsigned char decryptedMsgBuffer[MAXMSGLEN];
unsigned char sendProcessingBuffer[MAXMSGLEN];
mbedtls_gcm_context gcmCtx;


int main(void){
    //Helper. Remove later.
    uint32_t testNumber = 6;
    printf2("Endianess of number 6(uint32_t). Local: ");
    for(int i = 0; i<4; i++){
        printf("%d ",*((unsigned char*)&testNumber+i));
    }
    printf(" Network: ");
    testNumber = htonl(testNumber);
    for(int i = 0; i<4; i++){
        printf("%d ",*((unsigned char*)&testNumber+i));
    }
    printf("\n");



    //INITIALIZATION
    printf2("Initializing main server\n");
    //modifySetting("passExchangeMethod", 18, 0);
    initServer();

    //Test stuff

    // unsigned char testDevId[DEVIDLEN] = "TestTestTest1234";
    devInfo testDevInfo;
    memcpy(testDevInfo.devId,"TestTestTest1234",DEVIDLEN);
    memcpy(testDevInfo.key,"KeyKeyKeyKey1234",KEYLEN);
    addToList(&devInfos,testDevInfo.devId);

    commandRequestCommand_t testCommand;
    testCommand.opCode = 69; //poweroff the node
    testCommand.gSettings = 0;
    testCommand.argsPtr = NULL;
    testCommand.addPtr = NULL;
    addToCommandQueue("TestTestTest1234",&testCommand);


    pthread_create(&epollRecievingThreadID,NULL,epollRecievingThread,NULL);
    pthread_join(epollRecievingThreadID,NULL);
}

void initServer(){
    initBasicServerData(&connectionsInfo, &recvHolders, &devInfos, &commandsQueue, &globalSettings, 0);
    initServerCommandsList();
}

void deinitServer(){

}

//Processes a message, it can be any kind of message(node msg or user msg)
int processMsg(connInfo_t *connInfo, unsigned char *msg)
{
    printf2("Processing new message from socket: %d\n",connInfo->socket);
    //If caught beacon packet. Send it to the list of unverified devices, waiting for the user to accept/reject it. No persistent storage, just during runtime. Allow the option of instantly accepting all devices for debugging purposes.


    //MESSAGE STRUCTURE
    uint32_t msgLen = *(uint32_t*)msg;
    msgLen = ntohl(msgLen);
    uint32_t addMsgLen = *(uint32_t*)(msg+4);
    addMsgLen = ntohl(addMsgLen);
    uint32_t extraMsgLen = *(uint32_t*)(msg+8);
    extraMsgLen = ntohl(extraMsgLen);


    //Pointers to sections of the message
    unsigned char *msgLenPtr = msg;
    unsigned char *addLenPtr = msgLenPtr+4;
    unsigned char *extraLenPtr = addLenPtr+4;
    unsigned char *ivPtr = extraLenPtr+4;
    unsigned char *tagPtr = ivPtr+IVLEN;
    unsigned char *extraDataPtr = tagPtr+TAGLEN;
    unsigned char *extraDataPckPtr = extraDataPtr+protocolSpecificExtraDataLen;
    unsigned char *addDataPtr = extraDataPtr+extraMsgLen;
    unsigned char *addPckDataPtr = addDataPtr+protocolSpecificAddLen;
    unsigned char *encryptedDataPtr = addDataPtr+addMsgLen;
    unsigned char *encryptedPckDataPtr = encryptedDataPtr+protocolSpecificEncryptedDataLen;

    /* printf2("Message lengths - TotalLen: %d; AddLen: %d; ExtraLen: %d\n",msgLen,addMsgLen,extraMsgLen); */
    printMessage("Message structure(encrypted):",msg,0,true,true);


    //Extract the devID and hence its corresponding decryption key
    if(connInfo->connectionType==0){
        //Connection to node
        printf2("Processing a node message...\n");
        unsigned char devIdFromMessage[DEVIDLEN];

        devInfo *devInfo;
        pckDataToHostOrder(addPckDataPtr);
        if(getElementFromPckData(addPckDataPtr,devIdFromMessage,0)==-1){
            printf2("DEVID not present in ADD data, so ignore this message, since without DEVID we cant decrypt\n");
            return -1;
        }
        pckDataToNetworkOrder(addPckDataPtr);
        printf2("BOOM\n");

        // print2("DEVID FROM ADD DATA",devIdFromMessage,DEVIDLEN,0);


        for(int i = 0; i < devInfos.length; i ++){
            devInfo = getFromList(&devInfos,i);
            // printf("DEVINFO FOUND: %s\n",newDevInfo->devId);
            // printf("DEVINFO SENT: %s\n",devId);
            if(memcmp(devInfo->devId,devIdFromMessage,DEVIDLEN)==0){
                //Found the devID! Get key and dev type
                break; //So that the devInfo entry remains assigned
                // printf2("Found corresponding DEVID for the recieved message. Obtained corresponding decryption key\n");
            }else{
                printf2("No device with corresponding DEVID is known to server... Discard message\n");
                return -1;
            }
        }
        if(devInfos.length == 0){
            printf2("No device infos available. Cant decrypt, ignoring this message\n");
            return -1;
        }

        printf2("Message's DEVID: %.16s, KEY: %.16s\n",devInfo->devId,devInfo->key);

        //Decrypt the message
        initGCM(&gcmCtx,devInfo->key,KEYLEN*8);
        if(decryptPckData(&gcmCtx,msg,decryptedMsgBuffer)!=0){
            //Failed decryption (Could be spoofed ADD (DevId for instance) or something else, discard the message)
            printf2("Failed decrypting the message. Ignoring this message\n");
            return -1;
        }
        printf2("Message succesfully decrypted\n");
        //Copy the outputBuf(decrypted part of message) into the original message buffer, that will make it possible to use our printMessage function again on the decrypted message(Only doing this to make it easier to visually debug messages)
        uint32_t msgLenBeforeEncryptedData = 4+4+4+IVLEN+TAGLEN+extraMsgLen+addMsgLen;
        memcpy(msg+msgLenBeforeEncryptedData,decryptedMsgBuffer,msgLen-msgLenBeforeEncryptedData);
        printMessage("Message structure(decrypted):",msg,0,false,false);

        if(connInfo->localNonce==0 && connInfo->remoteNonce==0){
            //No nonce sent but we just recieved the other side's nonce. Hence send ours.
            //getrandom(&(connInfo->localNonce),4,0); //Generate random nonce
            //if(connInfo->localNonce==0){
            //    //Just in case the random number is 0, since 0 signifies no nonce was set
            //    connInfo->localNonce = 1;
            //}

            //Since we are recieving a message and no nonce from our side has been sent, the other side must have sent their nonce on this message, hence get it.
            uint32_t remoteNonce = *((uint32_t*)encryptedDataPtr);
            connInfo->remoteNonce = remoteNonce;

            //Send off our part of nonce to remote (node)
            printf2("Sending off local nonce to node\n");
            //Generate dummy message
            unsigned char *pckDataEncrypted = NULL;
            unsigned char *pckDataADD = NULL;
            unsigned char *pckDataExtra = NULL;

            encryptAndSendAll(connInfo->socket,0,connInfo,&gcmCtx,pckDataEncrypted,pckDataADD,pckDataExtra,0,sendProcessingBuffer);

            //SET SESSION ID AFTER SENDING OFF THE NONCE, OTHERWISE THE encryptAndSendAll() function will think that sessioID was establish and misbehave!!
            connInfo->sessionId = connInfo->remoteNonce + connInfo->localNonce;  //GENERATING THE NEW SESSION ID BY ADDING UP THE NONCES

            printf2("Nonce local: %u; Nonce remote: %u; Generated sessionID: %u\n", connInfo->localNonce, connInfo->remoteNonce, connInfo->sessionId);
            printf2(ANSI_COLOR_BLUE "SessionID established. SessionID: %u\n" ANSI_COLOR_RESET,connInfo->sessionId);

            //NOW SEND OFF ANY MESSAGES IN THE QUEUE
            sendQueuedMessages(connInfo,devInfo);
            printf2("Closing the connection\n");
            close1(connInfo->socket,&connectionsInfo);

        }else if(connInfo->remoteNonce==0 && connInfo->localNonce!=0){
            //We sent our nonce before but have just recieved the nonce of the other side

            uint32_t *remoteNonce = (uint32_t*)encryptedDataPtr;
            connInfo->remoteNonce = *remoteNonce;
            
            connInfo->sessionId = connInfo->remoteNonce + connInfo->localNonce;  //GENERATING THE NEW SESSION ID BY ADDING UP THE NONCES

            printf2(ANSI_COLOR_BLUE "SessionID established\n" ANSI_COLOR_RESET);
            printf2("Nonce local: %d; Nonce remote: %d; Generated sessionID: %d\n", connInfo->localNonce, connInfo->remoteNonce, connInfo->sessionId);

            //NOW SEND OFF ANY MESSAGES IN THE QUEUE
            sendQueuedMessages(connInfo,devInfo);
            printf2("Closing the connection\n");
            close1(connInfo->socket,&connectionsInfo);

        }else if(connInfo->sessionId!=0){
            //Process actual messages
            printf2("PROCESSING ACTUAL MESSAGE\n");
            uint32_t devtype, opcode, argsLen;
            getElementFromPckData(encryptedPckDataPtr,(unsigned char*)&devtype,0);
            getElementFromPckData(encryptedPckDataPtr,(unsigned char*)&opcode,1);
            argsLen = getElementLengthFromPckData(encryptedPckDataPtr,2);
            unsigned char args[argsLen];
            getElementFromPckData(encryptedPckDataPtr,args,2);

            //Serialization
            devtype = ntohl(devtype);
            opcode = ntohl(opcode);

            printf2("devtype: %d; opcode: %d; argsLen: %d\n",devtype,opcode,argsLen);

            if(opcode<=nodeToServerReservedOpCodesUpperLimit){
                //Reserved opcode henc devtype=0
                /* printf2("Executing reserved opcode\n"); */
                serverCommands[0][opcode](args,argsLen);
            }else{
                /* printf2("Executing non-reserved opcode\n"); */
                serverCommands[devtype][opcode](args,argsLen);
            }

            /* if(devtype==1){ */
            /*     //TEST DEVICE */
            /*     if(opcode==0){ */
            /*         //Beacon packet */
            /*         printf2("Beacon packet recieved. (Data:%s)\n",args); */
            /*     } */
            /* } */
        }
    }

    //Clear out message buffer
    memset(msg,0,MAXMSGLEN);

}

//Empties the server message queue for specific device and sends them all off to corresponding device
void sendQueuedMessages(connInfo_t *connInfo, devInfo *devInfo){
    printf2("Sending off queued messages\n");
    printf2("Command queue length: %d\n",commandsQueue.length);
    for(int i = 0; i <commandsQueue.length; i++){
        commandRequest_t *commandsRequest = getFromList(&commandsQueue,i);
        /* printf2("our DEVID: %s. Stored DEVID: %s\n",commandsRequest->devId,devInfo->devId); */
        if((memcmp(commandsRequest->devId,devInfo->devId,DEVIDLEN))==0){
            //Found corresponding commandsRequest to our DEVID
            arrayList *commandsList = &(commandsRequest->commandRequestsCommands);
            for(int i = 0; i< commandsList->length; i++){
                //Send off all commands from it
                commandRequestCommand_t *command = getFromList(commandsList,i);
                sendOffCommand(command,connInfo,devInfo->devId);
            }

            //Now remove these messages from queue after sending them
            removeFromList(&commandsQueue,i);
        }
    }

}

void addToCommandQueue(unsigned char *DEVID, commandRequestCommand_t *command){
    for(int i = 0; i<commandsQueue.length; i++){
        commandRequest_t *commandRequest = getFromList(&commandsQueue,i);
        if((memcmp(commandRequest->devId,DEVID,DEVIDLEN))==0){
            //Add to existing commandRequest
            addToList(&(commandRequest->commandRequestsCommands),command);
        }
    }
    
    //If no corresponding commandRequest for the DEVID was found. Create a new one and add the command
    commandRequest_t newCommandRequest;
    commandRequest_t *newCommandRequestRef = NULL;
    addToList(&commandsQueue,&newCommandRequest);
    newCommandRequestRef = getFromList(&commandsQueue,commandsQueue.length-1);
    memcpy(newCommandRequestRef->devId,DEVID,DEVIDLEN);
    arrayList newCommandList;
    initList(&newCommandList,sizeof(commandRequestCommand_t));
    newCommandRequestRef->commandRequestsCommands = newCommandList;
    addToList(&(newCommandRequestRef->commandRequestsCommands),command);


}

void sendOffCommand(commandRequestCommand_t *command, connInfo_t *connInfo, unsigned char *devId){
    printf2("Sending off command with opcode: %d\n",command->opCode);
    unsigned char *encryptedPckDataPtr = NULL;
    unsigned char *addPckDataPtr = command->addPtr;
    unsigned char *extraPckDataPtr = NULL;

    initPckData(&encryptedPckDataPtr);
    appendToPckData(&encryptedPckDataPtr,(unsigned char*)&(command->opCode),sizeof(uint32_t));
    if(command->argsPtr!=NULL){
        appendToPckData(&encryptedPckDataPtr,command->argsPtr,command->argsLen);
    }
    unsigned char *key = findEncryptionKey(devId);
    initGCM(&gcmCtx,key,KEYLEN*8);
    encryptAndSendAll(connInfo->socket,0,connInfo,&gcmCtx,encryptedPckDataPtr,addPckDataPtr,extraPckDataPtr,command->gSettings,sendProcessingBuffer);

}

//Returns the encryption key for the corresponding DEVID by searching through devInfos arrayList
unsigned char * findEncryptionKey(unsigned char *DEVID){
    for(int i = 0; i<devInfos.length; i++){
        devInfo *devInfo = getFromList(&devInfos,i);
        if((memcmp(devInfo->devId,DEVID,DEVIDLEN))==0){
            //Found the key
            return devInfo->key;
        }
    }
    return NULL; //No key found
}

//Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...){
    int result;
    va_list args;
    va_start(args,formattedInput);
    printf(ANSI_COLOR_GREEN "mainServer: " ANSI_COLOR_RESET);
    result = vprintf(formattedInput,args);
    va_end(args);
    return result;
}

