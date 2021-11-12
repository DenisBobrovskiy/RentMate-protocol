#ifndef SOCKETTHREAD_HEADER_H
#define SOCKETTHREAD_HEADER_H
#include "compileFlag.h"

#if targetPlatform==1
#include "pckData/pckData.h"
#elif targetPlatform==0
#include "pckData.h"
#endif

extern connInfo_t connInfo;

static int printf2(char *formattedInput, ...);
void *socketThread(void *args);
void initializeSocket();



#endif
