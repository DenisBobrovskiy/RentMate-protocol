#ifndef SERVER_HEADER_H
#define SERVER_HEADER_H

#include <stdint.h>
#include "arrayList.h"
#include "pckData.h"

extern pthread_mutex_t commandsQueueMux;
extern arrayList connectionsInfo;
extern arrayList recvHolders;
extern arrayList devInfos; //Stores information for every device known to the server

//Local settings file for the local server device
typedef struct _localSettingsStruct{
    char username[30];
    char propertyId[24];
    char propertyKey[12];
}localSettingsStruct;

extern localSettingsStruct localSettings;

int modifySetting(unsigned char *settingName, int settingLen, uint32_t newOption);
static int printf2(char *formattedInput, ...);
int processMsg(connInfo_t *connInfo, unsigned char *msg);
void initServer();
void deinitServer();

void sendQueuedMessages(connInfo_t *connInfo, devInfo *devInfo);
void addToCommandQueue(unsigned char *DEVID, commandRequestCommand_t *command);
void sendOffCommand(commandRequestCommand_t *command, connInfo_t *connInfo, unsigned char *devId);

unsigned char * findEncryptionKey(unsigned char *DEVID);

int loadInServerSettings();
int saveDevInfoToConfigFile(devInfo *data);

#endif
