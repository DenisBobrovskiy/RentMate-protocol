#ifndef SOCKETTHREAD_HEADER_H
#define SOCKETTHREAD_HEADER_H
#include "compileFlag.h"

#define MAX_RECONNECT_ATTEMPTS 5 //If connection fails more times than this number, reset Controller's IP and wait for UDP broadcast to get new ip


#if targetPlatform==1
#include "pckData.h"
#elif targetPlatform==2
#include "pckData.h"
#endif

extern connInfo_t connInfo;

static int printf2(char *formattedInput, ...);
void *socketThread(void *args);
int initializeSocket();



#endif
