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
    //INITIALIZATION
    printf2("Initializing main server\n");
    //modifySetting("passExchangeMethod", 18, 0);
    initBasicServerData(&connectionsInfo, &recvHolders, &devInfos, &commandsQueue, &globalSettings, 0);

    //Test stuff
    
    // unsigned char testDevId[DEVIDLEN] = "TestTestTest1234";
    devInfo testDevInfo;
    memcpy(testDevInfo.devId,"TestTestTest1234",DEVIDLEN);
    memcpy(testDevInfo.key,"KeyKeyKeyKey1234",KEYLEN);
    addToList(&devInfos,testDevInfo.devId);


    pthread_create(&epollRecievingThreadID,NULL,epollRecievingThread,NULL);
    pthread_join(epollRecievingThreadID,NULL);
}

//Processes a message, it can be any kind of message(node msg or user msg)
int processMsg(connInfo_t *connInfo, unsigned char *msg)
{
    printf2("Processing new message from socket: %d\n",connInfo->socket);
    //If caught beacon packet. Send it to the list of unverified devices, waiting for the user to accept/reject it. No persistent storage, just during runtime. Allow the option of instantly accepting all devices for debugging purposes.


    //MESSAGE STRUCTURE
    printf2("Message structure:\n");
    for(int i = 0; i <MAXMSGLEN; i++){
        printf("%d ", *(msg+i));
    }
    printf("\n");


    //Extract the devID and hence its corresponding decryption key
    if(connInfo->connectionType==0){
        //Connection to node
        printf2("Processing a node message...\n");
        unsigned char *devIdFromMessage;

        devInfo *devInfo;

        //Get devId from the message's ADD data (not verified yet, verify that it hasnt been spoofed after decryption!!)
        arrayList tempAddData;
        arrayList decryptedMsg;
        readAddData(msg,&tempAddData);
        // unsigned char *devIdDataEntry = getFromList(&tempAddData,0); //Returns a length of the entry (4 bytes) followed by a pointer to that data (pointer is inside the message buffer we passed to the readAddData() function)
        // uint32_t devIdDataLength = *(uint32_t*)devIdDataEntry;
        // unsigned char **devIdDataEntryPtr = devIdDataEntry+4;
        // devIdFromMessage = *devIdDataEntryPtr;  //Here we have obtained a pointer to the DEVID in the message buffer
        if(readDataEntry(&tempAddData,&devIdFromMessage,0)==-1){
            printf2("DEVID not present in ADD data, so ignore this message, since without DEVID we cant decrypt\n");
            return -1;
        }
        print2("DEVID FROM ADD DATA",devIdFromMessage,DEVIDLEN,0);


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
        freeArrayList(&tempAddData);

        printf2("Message's DEVID: %.16s, KEY: %.16s\n",devInfo->devId,devInfo->key);

        //Decrypt the message
        initGCM(&gcmCtx,devInfo->key,KEYLEN*8);
        if(decryptPckData(&gcmCtx,msg,decryptedMsgBuffer)!=0){
            //Failed decryption (Could be spoofed ADD (DevId for instance) or something else, discard the message)
            printf2("Failed decrypting the message. Ignoring this message\n");
            return -1;
        }
        printf2("Message succesfully decrypted\n");

        if(connInfo->localNonce==0 && connInfo->remoteNonce==0){
            //No nonce sent but we just recieved the other side's nonce. Hence send ours.
            //getrandom(&(connInfo->localNonce),4,0); //Generate random nonce
            //if(connInfo->localNonce==0){
            //    //Just in case the random number is 0, since 0 signifies no nonce was set
            //    connInfo->localNonce = 1;
            //}

            //Since we are recieving a message and no nonce from our side has been sent, the other side must have sent their nonce on this message, hence get it.
            uint32_t remoteNonce = getNonceFromDecryptedData(decryptedMsgBuffer);
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

        }else if(connInfo->remoteNonce==0 && connInfo->localNonce!=0){
            //We sent our nonce before but have just recieved the nonce of the other side

            uint32_t *remoteNonce = getFromList(&decryptedMsg,0);
            connInfo->remoteNonce = *remoteNonce;
            
            connInfo->sessionId = connInfo->remoteNonce + connInfo->localNonce;  //GENERATING THE NEW SESSION ID BY ADDING UP THE NONCES

            printf2(ANSI_COLOR_BLUE "SessionID established\n" ANSI_COLOR_RESET);
            printf2("Nonce local: %d; Nonce remote: %d; Generated sessionID: %d\n", connInfo->localNonce, connInfo->remoteNonce, connInfo->sessionId);

            //NOW SEND OFF ANY MESSAGES IN THE QUEUE
        }
    }



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

