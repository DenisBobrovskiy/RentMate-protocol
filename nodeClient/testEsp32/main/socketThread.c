#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <netdb.h>
#include <memory.h>
#include <pthread.h>
#include "client.h"
#include <sys/random.h>
#include <unistd.h>
#include <sys/socket.h>
#include "compileFlag.h"


#if targetPlatform==1
#include "pckData/pckData.h"
#include "pckData/generalSettings.h"
#elif targetPlatform==0
#include "pckData.h"
#include "generalSettings.h"
#endif


#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_WHITE  "\x1B[37m"

connInfo_t connInfo;
unsigned char sendProcessingBuffer[MAXMSGLEN];
unsigned char recvProcessingBuffer[MAXMSGLEN];
int yes = 1;
int socketMain;
char targetIP[INET_ADDRSTRLEN] = "192.168.4.4";
uint16_t port = 3333;
unsigned char pointerToKey[16] = "KeyKeyKeyKey1234";
struct sockaddr_in newConnAddr;
socklen_t sockLen;

void initializeSocket(){
        initGCM(&encryptionContext, pointerToKey, KEYLEN*8);
        //INITIALIZING AND CONNECTING
        printf2("Initializing the socket\n");
        socketMain = socket(AF_INET, SOCK_STREAM, 0);
        initializeConnInfo(&connInfo,socketMain);
        //setsockopt(socketMain, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        newConnAddr.sin_addr.s_addr = inet_addr(targetIP);
        newConnAddr.sin_family = AF_INET;
        newConnAddr.sin_port = htons(port);
        sockLen = sizeof(newConnAddr);
}

void *socketThread(void *args){
    
    bool connectionClosedByServer;
    //establish connection at wake up intervals (adjustable)
    while(1){
        connectionClosedByServer = false;
        initializeSocket();
        if(connect(socketMain,(struct sockaddr*)&newConnAddr,sockLen)==-1){
            printf2("Connection failed\n");
            exit(1);
        }
        printf2("Connected succesfully\n");
        //Establish sessionID
        if(connInfo.localNonce==0){
            //Generate a localNonce
            unsigned char *encryptedPckData = NULL;
            unsigned char *addPckData;
            //For ADD pck data we need DEVID so we can identify which device we are talking to
            initPckData(&addPckData);
            appendToPckData(&addPckData,(unsigned char*)&(localNodeSettings.devId),DEVIDLEN);
            unsigned char *extraPckData = NULL;


            if(encryptAndSendAll(socketMain,0,&connInfo,&encryptionContext,encryptedPckData,addPckData,extraPckData,0,sendProcessingBuffer)==-1){
                printf2("Failed sending the localNonce...\n");
            }

            //Wait for remote nonce
            while(connInfo.sessionId==0){
                printf2("Waiting\n");
                printf2("Waiting to establish sessionID\n");
                int status = recvAll(&recvHolders,&connInfo,socketMain,recvProcessingBuffer,processMsg);
                if(status==0){
                    //Connection was closed
                    /* printf2("Connection closed by server\n"); */
                    /* close(socketMain); */
                    /* connectionClosedByServer = true; */
                    //NOT CLOSED IT JUST GETS 0 BYTES AT THE END OF THE LOOP
                    break;
                }else if(status==-1){
                    //Error occured
                    printf2("Error recieving the message\n");
                    break;
                }
            }
            
        }

        //Check if sessionID (and therefore connection) is established and then send out any pending commands and recieve any pending commands
        if(connInfo.sessionId!=0){
            nodeCmdInfo currentCmdInfo;
            unsigned char *pckDataEncrypted;
            unsigned char *pckDataAdd;
            unsigned char outBuf[MAXMSGLEN];
            uint32_t tempGSettings = 0; //For now have empty settings (until i actually add settings)
            int numOfCommands = getCommandQueueLength(&nodeCommandsQueue);
            printf2("Num of commands to send out: %d\n",numOfCommands);
            for (int i = 0; i<numOfCommands; i++){
                popCommandQueue(&nodeCommandsQueue,&currentCmdInfo);
                print2("DEVID TO BE SENT",currentCmdInfo.devId,16,0);
                composeNodeMessage(&currentCmdInfo,&pckDataEncrypted,&pckDataAdd);
                encryptAndSendAll(socketMain,0,&connInfo,&encryptionContext,pckDataEncrypted,pckDataAdd,NULL,tempGSettings,outBuf);
                
            }

            //Recieve any pending commands
            int ret = recvAll(&recvHolders,&connInfo,socketMain,recvProcessingBuffer,processMsg);
            if(ret==1){
                //Connection closed
                initializeConnInfo(&connInfo,0);
                printf2("Connection closed from server\n");
                close(socketMain);
                connectionClosedByServer = true;
            }

        }else {
            //Connection not established, do nothing
        }

        //Close connection
        if(!connectionClosedByServer){
            close(socketMain);
        }
        sleep_ms(wakeUpIntervalMs);
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
