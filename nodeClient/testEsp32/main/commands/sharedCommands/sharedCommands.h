#ifndef HEADER_SHAREDCOMMANDS_H
#define HEADER_SHAREDCOMMANDS_H

#include "../../compileFlag.h"

#include <stdint.h>
#include <stdio.h>

#include "generalSettings.h"

#if targetPlatform==1
#include "pckData.h"
#elif targetPlatform==2
#include "pckData.h"
#endif

//Here define all the functions for server commands
typedef int (*commandPtr)(unsigned char *args, uint32_t argsLen);  //Command pointers

extern commandPtr *nodeCommands;

void initSharedNodeCommands(int maxUserCommands);

int serverToNodeMessage(unsigned char *args, uint32_t argsLen);

static int printf2(char *formattedInput, ...);


#endif
