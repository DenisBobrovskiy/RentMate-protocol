#ifndef HEADER_COMMANDSSERVER_H
#define HEADER_COMMANDSSERVER_H
#include "pckData/pckData.h"

//Here define all the functions for server commands
typedef int (*commandPtr)(unsigned char *args, uint32_t argsLen);  //Command pointers

extern commandPtr **serverCommands;


int nodeToServerBeacon(unsigned char *args, uint32_t argsLen);


void initServerCommandsList();
static int printf2(char *formattedInput, ...);

#endif
