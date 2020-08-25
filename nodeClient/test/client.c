#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "pckData/pckData.h"
#include <time.h>
#include "pckData/generalSettings.h"
#include <memory.h>
#include <string.h>
#include "client.h"

#define ANSI_COLOR_BLUE  "\x1B[34m"
#define ANSI_COLOR_WHITE  "\x1B[37m"

char *nodeSettingsFileName = "settings.conf";
nodeSettings_t localNodeSettings;
connInfo_t connInfo;
unsigned char sendProcessingBuffer[MAXMSGLEN];

int main(){
    int yes = 1;
    int socketMain;
    char targetIP[INET_ADDRSTRLEN] = "127.0.0.1";
    uint16_t port = 3333;
    mbedtls_gcm_context encryptionContext;
    unsigned char pointerToKey[16] = "ABDFKfjrkffkrifj";

    //LOAD IN THE settings.conf FILE and set the devId, devtype etc...
    loadInNodeSettings();
    // print2("DEVID: ",localNodeSettings.devId,DEVIDLEN,0);
    initGCM(&encryptionContext, pointerToKey, KEYLEN*8);


    //INITIALIZING AND CONNECTING
    printf2("Initializing the socket\n");
    socketMain = socket(AF_INET, SOCK_STREAM, 0);
    initializeConnInfo(&connInfo,socketMain);
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

    //Send a test message
    unsigned char *pckDataEncrypted;
    unsigned char *pckDataAdd;
    composeBeaconPacket((unsigned char*)"Test beacon",12,&pckDataEncrypted,&pckDataAdd);


    uint32_t pckDataLen = *(uint32_t*)pckDataEncrypted;
    print2("Add pck data to be added to message:",pckDataAdd,28,0);
    // sleep_ms(500);
    encryptAndSendAll(socketMain,0,&connInfo,&encryptionContext,pckDataEncrypted,pckDataAdd,NULL,0,sendProcessingBuffer);
    printf2("Msg sent\n");

    // while(1){
    //     sleep_ms(500);
    //     send(socketMain,tempMsg,8,0);
    // }
}

//Composes a generic message (not fully formatted for the protocol, use pckData functions to complete the  message) use this for functions that compose different message types with different arguments
int composeNodeMessage(nodeCmdInfo *currentNodeCmdInfo, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd){
    initPckData(pckDataAdd);
    initPckData(pckDataEncrypted);

    appendToPckData(pckDataAdd,currentNodeCmdInfo->devId,16);

    appendToPckData(pckDataEncrypted,(unsigned char*)&(currentNodeCmdInfo->devType),4);
    appendToPckData(pckDataEncrypted,(unsigned char*)&(currentNodeCmdInfo->opcode),4);
    appendToPckData(pckDataEncrypted, (unsigned char*)&(currentNodeCmdInfo->argsLen),4);
    appendToPckData(pckDataEncrypted, (unsigned char*)&(currentNodeCmdInfo->args),currentNodeCmdInfo->argsLen);
    
    uint32_t pckDataLen = *(uint32_t*)pckDataEncrypted;
    // printf2("pointer: %p\n",pckDataEncrypted);
    // printf2("BOOOOM: %d\n",pckDataLen);
    // pckDataToNetworkOrder(pckDataAdd);
    // pckDataToNetworkOrder(pckDataEncrypted);
    return 0;
}


int composeBeaconPacket(unsigned char *beaconData, uint8_t beaconDataLen, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd){
    nodeCmdInfo currentNodeCmdInfo;
    memcpy(currentNodeCmdInfo.devId,localNodeSettings.devId,DEVIDLEN);
    currentNodeCmdInfo.devType = localNodeSettings.devType;
    currentNodeCmdInfo.opcode = 0;
    currentNodeCmdInfo.argsLen = beaconDataLen;
    currentNodeCmdInfo.args = beaconData;
    composeNodeMessage(&currentNodeCmdInfo, pckDataEncrypted, pckDataAdd);

    return 1;
}


int loadInNodeSettings(){
    //Open the file
    FILE *settingsFile;
	char temp[60];
	int currentCharacters;


    if((settingsFile = fopen(nodeSettingsFileName,"r"))==NULL){
        // printf("Failed opening node settings file\n");
        return -1;
    }

    //Analyze every line
	while(fgets(temp,60,settingsFile)!=NULL){
		currentCharacters = strcspn(temp,"\n");
		char *settingPtr = temp;
        char *settingName;
        char *settingValue;
        char *settingNameBegin;
        char *valueNameBegin;
        int settingsNamePassedFirstQuote = 0;
        int settingNameComplete = 0;
        char *settingValueBegin;
        int settingValuePassedFirstQuote = 0;
        int settingValueComplete = 0;

        printf2("Loading in node settings...\n");

		for(int i = 0; i<currentCharacters;i++){
            if(*(temp+i)=='"' && settingsNamePassedFirstQuote==1 && settingNameComplete!=1){
                //Malloc space for setting name (and later value), remember to dealloc
                int settingNameLen =(temp+i)-settingNameBegin-1;
                settingName = malloc(settingNameLen);
                memcpy(settingName, settingNameBegin+1, settingNameLen);
                *(settingName+settingNameLen) = 0;
                settingNameComplete = 1;
                
            }else if(*(temp+i)=='"' && settingsNamePassedFirstQuote==0 && settingNameComplete!=1){
                settingNameBegin = temp+i;
                settingsNamePassedFirstQuote=1;
            }else if(*(temp+i)=='"' && settingValuePassedFirstQuote==1 && settingNameComplete==1){
                int settingValueLen = (temp+i)-settingValueBegin-1;
                settingValue = malloc(settingValueLen);
                memcpy(settingValue, settingValueBegin+1, settingValueLen);
                *(settingValue+settingValueLen) = 0;
                settingValueComplete = 1;
                // printf("settings value len: %d\n",settingValueLen);
                // print2("setting original",settingValueBegin+1,DEVIDLEN,0);
                // print2("setting copied",settingValue,DEVIDLEN,0);
            }else if(*(temp+i) == '"' && settingValuePassedFirstQuote == 0 && settingNameComplete==1){
                settingValueBegin = temp+i;
                settingValuePassedFirstQuote = 1;
            }

            if(settingValueComplete==1){
                break;
            }
            
        }
        printf2("NAME: %s\n",settingName);
        printf2("VALUE: %s\n",settingValue);
        //Analyze the obtained setting name and values and dealloc
        if(strcmp(settingName,"devId")==0){
            memcpy(localNodeSettings.devId,settingValue,DEVIDLEN);
        }else if(strcmp(settingName,"devType")==0){
            localNodeSettings.devType = atoi(settingName);
        }
        free(settingName);
        free(settingValue);
    }
    return 1;
}

int initializeConnInfo(connInfo_t *connInfo, int socket){
    connInfo->connectionType = 0;
    connInfo->localNonce = 0;
    connInfo->remoteNonce = 0;
    connInfo->rcvSessionIncrements = 0;
    connInfo->sendSessionIncrements = 0;
    connInfo->sessionId = 0;
    connInfo->socket = socket;

    return 0;
}


void sleep_ms(int millis){
    //printf("Sleeping for %i ms\n",millis);
    struct timespec ts;
    ts.tv_sec = millis/1000;
    ts.tv_nsec = (millis%1000) * 1000000;
    nanosleep(&ts, NULL);
}

//Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...){
    int result;
    va_list args;
    va_start(args,formattedInput);
    printf(ANSI_COLOR_BLUE "NodeClient: " ANSI_COLOR_WHITE);
    result = vprintf(formattedInput,args);
    va_end(args);
    return result;
}
