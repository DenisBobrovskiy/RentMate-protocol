#ifndef HEADER_SMARTLOCKCOMMANDS_H
#define HEADER_SMARTLOCKCOMMANDS_H

#include "../../compileFlag.h"

#include <stdint.h>

#include "generalSettings.h"
#include "../sharedCommands/sharedCommands.h"

#if targetPlatform==1
#include "pckData.h"
#elif targetPlatform==2
#include "pckData.h"
#endif

void initNodeCommands();

int changePasscode(unsigned char *args, uint32_t argsLen);

static int printf2(char *formattedInput, ...);



#endif
