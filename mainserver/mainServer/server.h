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

#endif
