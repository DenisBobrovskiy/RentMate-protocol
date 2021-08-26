#ifndef SERVER_HEADER_H
#define SERVER_HEADER_H

#include <stdint.h>
#include "ArrayList/arrayList.h"
#include "pckData/pckData.h"

extern arrayList connectionsInfo;
extern arrayList recvHolders;

int modifySetting(unsigned char *settingName, int settingLen, uint32_t newOption);
static int printf2(char *formattedInput, ...);
int processMsg(connInfo_t *connInfo, unsigned char *msg);
void initServer();
void deinitServer();

void sendQueuedMessages(connInfo_t *connInfo, devInfo *devInfo);
void addToCommandQueue(unsigned char *DEVID, commandRequestCommand_t *command);
void sendOffCommand(commandRequestCommand_t *command, connInfo_t *connInfo, unsigned char *devId);

unsigned char * findEncryptionKey(unsigned char *DEVID);

#endif
