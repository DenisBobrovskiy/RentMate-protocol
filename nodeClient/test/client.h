#ifndef TESTSENDER_HEADER_H
#define TESTSENDER_HEADER_H

#include <stdint.h>
#include "pckData/pckData.h"


int loadInNodeSettings();

//Only used internally, dont expose to end program
typedef struct _nodeCmdInfo{
    uint64_t devId;
    uint16_t devType;
    uint16_t opcode;
    uint32_t argsLen;
    unsigned char *args;    //Pointer to arguments
}nodeCmdInfo;

typedef struct _globals{
    uint64_t devId;
    uint16_t devType;
}globals;

int composeNodeMessage(nodeCmdInfo *currentNodeCmdInfo, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd);

int composeBeaconPacket(unsigned char *beaconData, uint8_t beaconDataLen, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd);
int initializeConnInfo(connInfo_t *connInfo, int socket);
void sleep_ms(int millis);
static int printf2(char *formattedInput, ...);
#endif

