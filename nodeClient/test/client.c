#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "pckData/pckData.h"
#include "pckData/generalSettings.h"
#include <memory.h>
#include <string.h>
#include "client.h"

char *nodeSettingsFileName = "settings.conf";
globals localGlobals;


int main(){
    int yes = 1;
    int socketMain;
    char targetIP[INET_ADDRSTRLEN] = "127.0.0.1";
    uint16_t port = 4444;
    mbedtls_gcm_context encryptionContext;
    unsigned char *pointerToKey;


    //LOAD IN THE settings.conf FILE and set the devId, devtype etc...
    loadInNodeSettings();
    initGCM(&encryptionContext, pointerToKey, KEYLEN*8);


    //INITIALIZING AND CONNECTING
    socketMain = socket(AF_INET, SOCK_STREAM, 0);
    //setsockopt(socketMain, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    struct sockaddr_in newConnAddr;
    newConnAddr.sin_addr.s_addr = inet_addr(targetIP);
    newConnAddr.sin_family = AF_INET;
    newConnAddr.sin_port = htons(port);
    socklen_t sockLen = sizeof(newConnAddr);
    if(connect(socketMain,(struct sockaddr*)&newConnAddr,sockLen)==-1){
        printf("Connection failed\n");
        exit(1);
    }
    printf("Connected succesfully\n");

    //Send a test message
    unsigned char *pckDataEncrypted;
    unsigned char *pckDataAdd;
    composeBeaconPacket((unsigned char*)"Test beacon",12,pckDataEncrypted,pckDataAdd);
    
}

//Composes a generic message (not fully formatted for the protocol, use pckData functions to complete the  message) use this for functions that compose different message types with different arguments
int composeNodeMessage(nodeCmdInfo *currentNodeCmdInfo, unsigned char *pckDataEncrypted, unsigned char *pckDataAdd){
    initPckData(&pckDataAdd);
    initPckData(&pckDataEncrypted);
    appendToPckData(&pckDataAdd,(unsigned char*)&(currentNodeCmdInfo->devId),8);
    appendToPckData(&pckDataEncrypted,(unsigned char*)&(currentNodeCmdInfo->devType),2);
    appendToPckData(&pckDataEncrypted,(unsigned char*)&(currentNodeCmdInfo->opcode),2);
    appendToPckData(&pckDataEncrypted, (unsigned char*)&(currentNodeCmdInfo->argsLen),4);
    appendToPckData(&pckDataEncrypted, (unsigned char*)&(currentNodeCmdInfo->args),currentNodeCmdInfo->argsLen);
    pckDataToNetworkOrder(pckDataAdd);
    pckDataToNetworkOrder(pckDataEncrypted);
    return 0;
}


int composeBeaconPacket(unsigned char *beaconData, uint8_t beaconDataLen, unsigned char *pckDataEncrypted, unsigned char *pckDataAdd){
    nodeCmdInfo *currentNodeCmdInfo;
    currentNodeCmdInfo->devId = localGlobals.devId;
    currentNodeCmdInfo->devType = localGlobals.devType;
    currentNodeCmdInfo->opcode = 0;
    currentNodeCmdInfo->argsLen = beaconDataLen;
    currentNodeCmdInfo->args = beaconData;
    composeNodeMessage(currentNodeCmdInfo, pckDataEncrypted, pckDataAdd);

    return 1;
}


int loadInNodeSettings(){
    //Open the file
    FILE *settingsFile;
	char temp[40];
	int currentCharacters;


    if((settingsFile = fopen(nodeSettingsFileName,"r"))==NULL){
        printf("Failed opening node settings file\n");
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
            }else if(*(temp+i) == '"' && settingValuePassedFirstQuote == 0 && settingNameComplete==1){
                settingValueBegin = temp+i;
                settingValuePassedFirstQuote = 1;
            }

            if(settingValueComplete==1){
                break;
            }
            
        }
        printf("NAME: %s\n",settingName);
        printf("VALUE: %s\n",settingValue);
        //Analyze the obtained setting name and values and dealloc
        if(strcmp(settingName,"devId")){
            localGlobals.devId = atoi(settingValue);
        }else if(strcmp(settingName,"devType")){
            localGlobals.devId = atoi(settingValue);
        }
        free(settingName);
        free(settingValue);
    }
    return 1;
}
