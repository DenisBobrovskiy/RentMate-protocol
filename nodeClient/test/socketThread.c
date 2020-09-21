#include <stdint.h>
#include <stdio.h>
#include "pckData/pckData.h"
#include "pckData/generalSettings.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <netdb.h>
#include <memory.h>
#include <pthread.h>
#include "client.h"
#include <sys/random.h>


#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_WHITE  "\x1B[37m"

connInfo_t connInfo;
unsigned char sendProcessingBuffer[MAXMSGLEN];
unsigned char recvProcessingBuffer[MAXMSGLEN];

void *socketThread(void *args){


    //------HAVE THIS IF FIGURE OUT ANOTHER WAY FOR NODES TO GET MESSAGE WHILE SAVING POWER APART FROM GETTING STUFF IN REPLY TO BEACON PACKETS-----------

    //char recievePortStr[10];
    //struct addrinfo hints, *socketInfo, *p1;
    //int err;
    //int yes = 1;
    //int mainSocket;

    //printf2("Recieve thread started\n");

    ////Establish recieving port on node
    //sprintf(recievePortStr,"%d",NodeRecievePort);
    //memset(&hints,0,sizeof(hints));

    //hints.ai_family = AF_UNSPEC;
    //hints.ai_socktype = SOCK_STREAM;
    //hints.ai_flags = AI_PASSIVE;

    //if((err = getaddrinfo(NULL, recievePortStr, &hints, &socketInfo)) != 0){
    //    fprintf(stderr, "getaddrinfo %s\n", gai_strerror(err));
    //    pthread_exit((void*)1);
    //}
    
    //for(p1 = socketInfo; p1!=NULL; p1->ai_next){
        
    //    //Create a socket
    //    if((mainSocket = socket(p1->ai_family, p1->ai_socktype,p1->ai_protocol)) == -1){
    //        //Error while settings socket
    //        perror("Error settings Node socket: ");
    //        pthread_exit((void*)1);
    //    }

    //    //Make the sockets reusable (since a removed socket can occupy space for some time hence preventing the program from executing if it was run recently, this instruct to reuse those old sockets)
    //    if(setsockopt(mainSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
    //        perror("Error during setsockopt REUSEADDR on node socket");
    //        pthread_exit((void*)1);
    //    }

    //    //Bind the socket
    //    if(bind(mainSocket,p1->ai_addr,p1->ai_addrlen) == -1){
    //        perror("Error while binding node socket");
    //        pthread_exit((void*)1);
    //    }
    //    break;
    //}

    //freeaddrinfo(socketInfo);
    
    //if(p1==NULL){
    //    fprintf(stderr,"Failed finding node address");
    //    pthread_exit((void*)1);
    //}
    
    //if(listen(mainSocket,maxConcurentConnectionsToNode) == -1){
    //    perror("Failed starting to listen on node socket");
    //    pthread_exit((void*)1);
    //}

    //printf2("Recieve socket initialized\n");


    while(1){
        printf2("Device waking up\n");
        int yes = 1;
        int socketMain;
        char targetIP[INET_ADDRSTRLEN] = "127.0.0.1";
        uint16_t port = 3333;
        unsigned char pointerToKey[16] = "KeyKeyKeyKey1234";

        initGCM(&encryptionContext, pointerToKey, KEYLEN*8);


        //INITIALIZING AND CONNECTING
        printf2("Initializing the socket\n");
        socketMain = socket(AF_INET, SOCK_STREAM, 0);
        initializeConnInfo(&connInfo,socketMain);
        connInfo.localNonce = 0;
        //setsockopt(socketMain, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        struct sockaddr_in newConnAddr;
        newConnAddr.sin_addr.s_addr = inet_addr(targetIP);
        newConnAddr.sin_family = AF_INET;
        newConnAddr.sin_port = htons(port);
        socklen_t sockLen = sizeof(newConnAddr);
        if(connect(socketMain,(struct sockaddr*)&newConnAddr,sockLen)==-1){
            printf2("Connection failed\n");
            exit(1);
        }
        printf2("Connected succesfully\n");

        //Establish sessionID
        if(connInfo.localNonce==0){
            //Generate a localNonce
            getrandom(&(connInfo.localNonce),4,0);
            unsigned char *encryptedPckData = NULL;
            unsigned char *addPckData = NULL;
            unsigned char *extraPckData = NULL;

            //Send off local nonce
            encryptAndSendAll(socketMain,0,&connInfo,&encryptionContext,encryptedPckData,addPckData,extraPckData,0,sendProcessingBuffer);

            //Wait for remote nonce
            while(connInfo.sessionId==0){
                printf2("Waiting to establish sessionID\n");
                recvAll(&recvHolders,&connInfo,socketMain,recvProcessingBuffer,processMsg);
            }
            
        }



        //Send a test message
        // unsigned char *pckDataEncrypted;
        // unsigned char *pckDataAdd;
        // composeBeaconPacket((unsigned char*)"Test beacon",12,&pckDataEncrypted,&pckDataAdd);


        // uint32_t pckDataLen = *(uint32_t*)pckDataEncrypted;
        // print2("Add pck data to be added to message:",pckDataAdd,28,0);
        // // sleep_ms(500);
        // encryptAndSendAll(socketMain,0,&connInfo,&encryptionContext,pckDataEncrypted,pckDataAdd,NULL,0,sendProcessingBuffer);
        // printf2("Msg sent\n");

        sleep_ms(1000); //Imitate deep sleep on ESP32
    }

}

static int printf2(char *formattedInput, ...){
    int result;
    va_list args;
    va_start(args,formattedInput);
    printf(ANSI_COLOR_YELLOW "SocketThread: " ANSI_COLOR_WHITE);
    result = vprintf(formattedInput,args);
    va_end(args);
    return result;
}
