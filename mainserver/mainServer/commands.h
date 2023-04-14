#ifndef HEADER_COMMANDSSERVER_H
#define HEADER_COMMANDSSERVER_H
#include "pckData.h"

//Here define all the functions for server commands
typedef int (*commandPtr)(unsigned char *devId, uint8_t devtype, unsigned char *args, uint32_t argsLen);  //Command pointers

extern commandPtr **serverCommands;

//Reserved opcode
int nodeToServerBeacon(unsigned char *devId, uint8_t devtype, unsigned char *args, uint32_t argsLen);
int nodeToServerMessage(unsigned char *devId, uint8_t devtype, unsigned char *args, uint32_t argsLen);


void initServerCommandsList();
static int printf2(char *formattedInput, ...);

#endif
