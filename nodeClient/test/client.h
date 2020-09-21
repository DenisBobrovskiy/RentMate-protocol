#ifndef TESTSENDER_HEADER_H
#define TESTSENDER_HEADER_H

#include <stdint.h>
#include "pckData/pckData.h"

extern arrayList nodeCommandsQueue;
extern pthread_mutex_t commandQueueAccessMux;
extern mbedtls_gcm_context encryptionContext;
extern arrayList recvHolders;

int loadInNodeSettings();

//Only used internally, dont expose to end program
typedef struct _nodeCmdInfo{
    char devId[DEVIDLEN];
    uint32_t devType;
    uint32_t opcode;
    uint32_t argsLen;
    unsigned char *args;    //Pointer to arguments
}nodeCmdInfo;

typedef struct _nodeSettings_t{
    char devId[DEVIDLEN];
    uint32_t devType;
}nodeSettings_t;

int composeNodeMessage(nodeCmdInfo *currentNodeCmdInfo, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd);
int freeNodeCmdInfo(nodeCmdInfo *cmdInfo);

// int composeBeaconPacket(unsigned char *beaconData, uint8_t beaconDataLen, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd);
int composeBeaconPacket(nodeCmdInfo *cmdInfo, unsigned char *beaconData, uint8_t beaconDataLen);
int initializeConnInfo(connInfo_t *connInfo, int socket);
void sleep_ms(int millis);
static int printf2(char *formattedInput, ...);

int initializeCommandQueue(arrayList *commandQueue);
int popCommandQueue(arrayList *commandQueue, nodeCmdInfo *cmdInfo);
int addToCommandQueue(arrayList *commandQueue, nodeCmdInfo *cmdInfo);
int getCommandQueueLength(arrayList *commandQueue);
int processMsg(connInfo_t *connInfo, unsigned char *message);
#endif

