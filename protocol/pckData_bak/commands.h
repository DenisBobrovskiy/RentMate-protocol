#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "generalSettings.h"
#include <stdarg.h>

int initOpCodes(uint32_t devType);
int executeCommand(uint32_t devType, uint32_t opCode, unsigned char isReservedCmd, unsigned char *argsData, uint32_t argsLen);
int echo(unsigned char *args, uint32_t argsLen);
int beacon(unsigned char *args, uint32_t argsLen);
int poweroff(unsigned char *args, uint32_t argsLen);
static int printf2(char *formattedInput, ...);

#endif
