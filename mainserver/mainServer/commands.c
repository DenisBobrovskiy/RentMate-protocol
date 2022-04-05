#include "pckData.h"
#include <stdio.h>
#include "server.h"
#include "commands.h"

#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

commandPtr **serverCommands; //A 2d array of functions where index corresponds to the devtype and second index corresponds to opcode

void initServerCommandsList(){
    printf2("Initialized server commands list\n");
    serverCommands = malloc(maxDevTypes*sizeof(commandPtr));
    serverCommands[0] = malloc(nodeToServerReservedOpCodesUpperLimit*sizeof(commandPtr));
    serverCommands[1] = malloc(maxTestDeviceCommands*sizeof(commandPtr));
    
    serverCommands[0][0] = nodeToServerBeacon;
    serverCommands[0][1] = nodeToServerMessage;
}

void deinitServerCommandsList(){
    if(serverCommands!=NULL){
        free(serverCommands);
    }
}


//RESERVED OPCODE COMMANDS

int nodeToServerBeacon(unsigned char *args, uint32_t argsLen){
    printf2("Beacon command message: ");
    for(int i = 0; i<argsLen; i++){
        printf("%c",*(args+i));
    }
    printf("\n");
    return 0;
}

int nodeToServerMessage(unsigned char *args, uint32_t argsLen){
    printf2("Message command recieved; Message: ");
    for(int i = 0; i < argsLen; i++){
        printf("%c",*(args+i));
    }
    printf2("\n");
    return 0;
}


//Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...){
    int result;
    va_list args;
    va_start(args,formattedInput);
    printf(ANSI_COLOR_GREEN "mainServer(commands.c): " ANSI_COLOR_RESET);
    result = vprintf(formattedInput,args);
    va_end(args);
    return result;
}

