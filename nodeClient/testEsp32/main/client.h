#ifndef TESTSENDER_HEADER_H
#define TESTSENDER_HEADER_H

#include <stdint.h>
#include "compileFlag.h"

#if targetPlatform==1
#include "pckData/pckData.h"
#elif targetPlatform==2
#include "pckData.h"
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


int loadInNodeSettings();

//Only used internally, dont expose to end program
typedef struct _nodeCmdInfo{
    unsigned char devId[DEVIDLEN];
    uint32_t devType;
    uint32_t opcode;
    uint32_t argsLen;
    unsigned char *args;    //Pointer to arguments
}nodeCmdInfo;

typedef struct _nodeSettings_t{
    unsigned char devId[DEVIDLEN];
    uint32_t devType;
}nodeSettings_t;

extern arrayList nodeCommandsQueue;
extern pthread_mutex_t commandQueueAccessMux;
extern mbedtls_gcm_context encryptionContext;
extern arrayList recvHolders;
extern nodeSettings_t localNodeSettings;


void initClient();
void initWifiStationMode(void);
static void wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void initializeClient();

int composeNodeMessage(nodeCmdInfo *currentNodeCmdInfo, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd);
int freeNodeCmdInfo(nodeCmdInfo *cmdInfo);

// int composeBeaconPacket(unsigned char *beaconData, uint8_t beaconDataLen, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd);
int composeBeaconPacketCmdInfo(nodeCmdInfo *cmdInfo, unsigned char *beaconData, uint8_t beaconDataLen);
int initializeConnInfo(connInfo_t *connInfo, int socket);
void sleep_ms(int millis);
static int printf2(char *formattedInput, ...);

int initializeCommandQueue(arrayList *commandQueue);
int popCommandQueue(arrayList *commandQueue, nodeCmdInfo *cmdInfo);
int addToCommandQueue(arrayList *commandQueue, nodeCmdInfo *cmdInfo);
int getCommandQueueLength(arrayList *commandQueue);
void deserializeCmdInfo(nodeCmdInfo *output, unsigned char *input);
void serializeCmdInfo(unsigned char *output, nodeCmdInfo *input);
int processMsg(connInfo_t *connInfo, unsigned char *message);
#endif

