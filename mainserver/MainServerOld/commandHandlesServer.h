#ifndef COMMANDHANDLES_HEADER_H
#define COMMANDHANDLES_HEADER_H

#include <stdint.h>
#include <stdio.h>

int initOpCodes();
int executeCommand(uint32_t devType, uint32_t opCode, unsigned char isReservedCmd, unsigned char *argsData, uint32_t argsLen);
int addToCommands(uint32_t opCode, int (*commandPtr)(unsigned char *args, uint32_t argsLen));
int echo(unsigned char *args, uint32_t argsLen);
int beacon(unsigned char *args, uint32_t argsLen);

#endif
