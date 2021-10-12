#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <time.h>
#include <memory.h>
#include <string.h>
#include "client.h"
#include "socketThread.h"
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include "compileFlag.h"
#include "client.h"

#if targetPlatform==2
#include "aes-gcm.h"
#include "arrayList.h"
#include "pckData.h"
#elif targetPlatform==1
#include "pckData/pckData.h"
#include "pckData/generalSettings.h"

#endif

#if targetPlatform==2
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "pckData.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#endif

#define ANSI_COLOR_BLUE  "\x1B[34m"
#define ANSI_COLOR_WHITE  "\x1B[37m"

//WIFI DATA
#define networkSSID "testbench"
#define networkPassword "testbenchPassword"
#define wifiMaximumReconnectAttempts  5

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;




char *nodeSettingsFileName = "nodeSettings.conf";
nodeSettings_t localNodeSettings;
globalSettingsStruct globalSettings;
// connInfo_t connInfo;
// unsigned char sendProcessingBuffer[MAXMSGLEN];
pthread_t socketThreadID;
unsigned char decryptionBuffer[MAXMSGLEN];
mbedtls_gcm_context encryptionContext;

arrayList nodeCommandsQueue;
arrayList recvHolders;
pthread_mutex_t commandQueueAccessMux;

uint32_t sizeOfSerializedCmdInfo = 0;


int app_main(){

    //Main init function for all platforms
    initClient();

    memcpy(localNodeSettings.devId,"TestTestTest1234",DEVIDLEN);

    //Initialize wifi stack for esp32
    #if targetPlatform==2
    initWifiStationMode();
    #endif
    //Initialize node command queue
    pthread_mutex_init(&commandQueueAccessMux,NULL);
    initializeCommandQueue(&nodeCommandsQueue);
    nodeCmdInfo testBeaconCmdInfo;
    composeBeaconPacketCmdInfo(&testBeaconCmdInfo,(unsigned char *)"TestBeacon1",12);
    addToCommandQueue(&nodeCommandsQueue,&testBeaconCmdInfo);
    // print2("First item in cmd queue",getFromList(&nodeCommandsQueue,0),sizeOfSerializedCmdInfo,0);

    // print2("DEVID: ",localNodeSettings.devId,DEVIDLEN,0);
    //initGCM(&encryptionContext, pointerToKey, KEYLEN*8);


    ////INITIALIZING AND CONNECTING
    //printf2("Initializing the socket\n");
    //socketMain = socket(AF_INET, SOCK_STREAM, 0);
    //initializeConnInfo(&connInfo,socketMain);
    //connInfo.localNonce = 0;
    ////setsockopt(socketMain, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    //struct sockaddr_in newConnAddr;
    //newConnAddr.sin_addr.s_addr = inet_addr(targetIP);
    //newConnAddr.sin_family = AF_INET;
    //newConnAddr.sin_port = htons(port);
    //socklen_t sockLen = sizeof(newConnAddr);
    //if(connect(socketMain,(struct sockaddr*)&newConnAddr,sockLen)==-1){
    //    printf2("Connection failed\n");
    //    exit(1);
    //}
    //printf2("Connected succesfully\n");

    ////Send a test message
    //unsigned char *pckDataEncrypted;
    //unsigned char *pckDataAdd;
    //composeBeaconPacket((unsigned char*)"Test beacon",12,&pckDataEncrypted,&pckDataAdd);


    //uint32_t pckDataLen = *(uint32_t*)pckDataEncrypted;
    //print2("Add pck data to be added to message:",pckDataAdd,28,0);
    //// sleep_ms(500);
   //encryptAndSendAll(socketMain,0,&connInfo,&encryptionContext,pckDataEncrypted,pckDataAdd,NULL,0,sendProcessingBuffer);
    //printf2("Msg sent\n");

    pthread_create(&socketThreadID,NULL,socketThread,NULL);
    pthread_join(socketThreadID,NULL);

    return 0;

}

void initClient(){
    #if targetPlatform==1
    printf2("Initializing linux client");
    #elif targetPlatform==2
    printf2("Initializing esp32 client");
    //INIT NVS (storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //Initialize wifi stack for esp32
    initWifiStationMode();

    #endif

    //Load in settings
    loadInNodeSettings();
    initBasicClientData(&recvHolders,&globalSettings, localNodeSettings.devType);
    sizeOfSerializedCmdInfo = DEVIDLEN+4+4+4+sizeof(unsigned char*);
}



void initWifiStationMode(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifiEventHandler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifiEventHandler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = networkSSID,
            .password = networkPassword,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 networkSSID, networkPassword);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 networkSSID, networkPassword);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

static void wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < wifiMaximumReconnectAttempts) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void initializeClient(){
    sizeOfSerializedCmdInfo = DEVIDLEN+4+4+4+sizeof(unsigned char*);
}


int processMsg(connInfo_t *connInfo, unsigned char *message){
    decryptPckData(&encryptionContext,message,decryptionBuffer);
    printf2("Processing recieved message\n");


    //Since before any communication the client sends a beacon packet to server. If we get a message and sessionID is yet to be established it means the server sent its part of the sessionID.
    if(connInfo->sessionId==0){
        //Get the remote nonce and establish sessionID
        connInfo->remoteNonce = *((uint32_t*)decryptionBuffer);
        connInfo->sessionId = connInfo->localNonce+connInfo->remoteNonce;
        printf2(ANSI_COLOR_BLUE "SessionID established. SessionID: %u\n" ANSI_COLOR_WHITE,connInfo->sessionId);
        return 0;
    }else if(connInfo->sessionId!=0){
        //We received a server side message
        printf2("Recieved a message from server.\n");
        printMessage("Recieved message(decrypted)",message,0,false,false);
        return 0;
    }else{
        printf2("ERROR. SessionID is not found for a new message. (This path is not supposed to be accessible)");
            return 1;
    }
}

//Composes a generic message (not fully formatted for the protocol, use pckData functions to complete the  message) use this for functions that compose different message types with different arguments
int composeNodeMessage(nodeCmdInfo *currentNodeCmdInfo, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd){
    initPckData(pckDataAdd);
    initPckData(pckDataEncrypted);

    uint32_t devType = htonl(currentNodeCmdInfo->devType);
    uint32_t opcode = htonl(currentNodeCmdInfo->opcode);

    appendToPckData(pckDataAdd,(unsigned char*)currentNodeCmdInfo->devId,DEVIDLEN);

    appendToPckData(pckDataEncrypted,(unsigned char*)&(devType),4);
    appendToPckData(pckDataEncrypted,(unsigned char*)&(opcode),4);
    appendToPckData(pckDataEncrypted, (unsigned char*)(currentNodeCmdInfo->args),currentNodeCmdInfo->argsLen);

    printPckData(NULL,*pckDataEncrypted,false,2);
    printPckData(NULL,*pckDataAdd,false,0);

    
    // printf2("pointer: %p\n",pckDataEncrypted);
    // printf2("BOOOOM: %d\n",pckDataLen);
    // pckDataToNetworkOrder(pckDataAdd);
    // pckDataToNetworkOrder(pckDataEncrypted);
    // print2("Final composed node message add data",*pckDataAdd,150,0);
    return 0;
}


// int composeBeaconPacket(unsigned char *beaconData, uint8_t beaconDataLen, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd){
//     nodeCmdInfo currentNodeCmdInfo;
//     memcpy(currentNodeCmdInfo.devId,localNodeSettings.devId,DEVIDLEN);
//     currentNodeCmdInfo.devType = localNodeSettings.devType;
//     currentNodeCmdInfo.opcode = 0;
//     currentNodeCmdInfo.argsLen = beaconDataLen;
//     currentNodeCmdInfo.args = beaconData;
//     composeNodeMessage(&currentNodeCmdInfo, pckDataEncrypted, pckDataAdd);

//     return 0;
// }

int composeBeaconPacketCmdInfo(nodeCmdInfo *cmdInfo, unsigned char *beaconData, uint8_t beaconDataLen){
    memcpy(cmdInfo->devId,localNodeSettings.devId,DEVIDLEN);
    cmdInfo->devType = localNodeSettings.devType;
    cmdInfo->opcode = 0;
    cmdInfo->argsLen = beaconDataLen;
    cmdInfo->args = malloc(beaconDataLen);
    memcpy(cmdInfo->args,beaconData,beaconDataLen);
    return 0;
}

int freeNodeCmdInfo(nodeCmdInfo *cmdInfo){
    free(cmdInfo->args);
    return 0;
}


int loadInNodeSettings(){
    //Open the file

    #if targetPlatform==1
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
        char *settingName = 0;
        char *settingValue = 0;
        char *settingNameBegin = 0;
        int settingsNamePassedFirstQuote = 0;
        int settingNameComplete = 0;
        char *settingValueBegin = 0;
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
            localNodeSettings.devType = atoi(settingValue);
        }
        free(settingName);
        free(settingValue);
    }
    return 1;
    #elif targetPlatform==2
    printf2("Opening node settings file using esp32's filesystem");

    return 1;
    #endif
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

int initializeCommandQueue(arrayList *commandQueue){
    pthread_mutex_lock(&commandQueueAccessMux);
    initList(&nodeCommandsQueue,sizeOfSerializedCmdInfo);
    pthread_mutex_unlock(&commandQueueAccessMux);
    return 0;
}

//Gets the first item in queue and removes it from the queue
int popCommandQueue(arrayList *commandQueue, nodeCmdInfo *cmdInfo){
    pthread_mutex_lock(&commandQueueAccessMux);
    unsigned char *serializedCmdInfo;
    serializedCmdInfo = getFromList(commandQueue,0);
    deserializeCmdInfo(cmdInfo,serializedCmdInfo);
    removeFromList(commandQueue,0);
    pthread_mutex_unlock(&commandQueueAccessMux);
    return 0;
}

int addToCommandQueue(arrayList *commandQueue, nodeCmdInfo *cmdInfo){
    pthread_mutex_lock(&commandQueueAccessMux);
    unsigned char serializedCmdInfo[sizeOfSerializedCmdInfo];
    serializeCmdInfo(serializedCmdInfo,cmdInfo);
    addToList(commandQueue,serializedCmdInfo);
    pthread_mutex_unlock(&commandQueueAccessMux);
    return 0;
}

int getCommandQueueLength(arrayList *commandQueue){
    pthread_mutex_lock(&commandQueueAccessMux);
    int len = commandQueue->length;
    pthread_mutex_unlock(&commandQueueAccessMux);
    return len;
}

void sleep_ms(int millis){
    //printf("Sleeping for %i ms\n",millis);
    #if targetPlatform==1
    struct timespec ts;
    ts.tv_sec = millis/1000;
    ts.tv_nsec = (millis%1000) * 1000000;
    nanosleep(&ts, NULL);
    #elif targetPlatform==2
    vTaskDelay(millis/portTICK_RATE_MS);
    #endif
}

//The output structure should have size as variable: sizeOfSerializedCmdInfo.
void serializeCmdInfo(unsigned char *output, nodeCmdInfo *input){
    memcpy(output,input->devId,DEVIDLEN);
    *((uint32_t*)(output+DEVIDLEN)) = input->devType;
    *((uint32_t*)(output+DEVIDLEN +4)) = input->opcode;
    *((uint32_t*)(output+DEVIDLEN +4 +4)) = input->argsLen;
    memcpy(output+DEVIDLEN+4+4+4,&(input->args),sizeof(unsigned char*));

}

void deserializeCmdInfo(nodeCmdInfo *output, unsigned char *input){
    memcpy(output->devId,input,DEVIDLEN);
    output->devType = *((uint32_t*)(input+DEVIDLEN));
    output->opcode = *((uint32_t*)(input+DEVIDLEN+4));
    output->argsLen = *((uint32_t*)(input+DEVIDLEN+4+4));
    memcpy(&(output->args),input+DEVIDLEN+4+4+4,sizeof(unsigned char*));

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


