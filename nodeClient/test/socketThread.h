#ifndef SOCKETTHREAD_HEADER_H
#define SOCKETTHREAD_HEADER_H

#include "pckData/pckData.h"

extern connInfo_t connInfo;

static int printf2(char *formattedInput, ...);
void *socketThread(void *args);
void initializeSocket();



#endif
