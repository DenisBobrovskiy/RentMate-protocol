//FUNCTIONS TO EXECUTE WHEN RECIEVED COMMANDS WITH SPECIFIC COMMAND CODE ON SERVER FROM NODE

#include "commandHandlesServer.h"
#include "stdlib.h"
#include "generalSettings.h"

//DevType to its opCodes dictionary
typedef int (*commandPtr)(unsigned char *args, uint32_t argsLen);  //Command pointers
commandPtr **devTypeToCommands;

int initOpCodes(){
    devTypeToCommands = malloc(maxDevTypes*sizeof(commandPtr));
    devTypeToCommands[0] = malloc(sizeof(commandPtr)*maxCodeLockCommands);
    devTypeToCommands[1] = malloc(sizeof(commandPtr)*maxCodeLockCommands);

    for(int i = 0; i <maxDevTypes; i++){
        devTypeToCommands[i] = NULL;
    }
    //Commands
    devTypeToCommands[0] = malloc(maxReservedCommands*sizeof(commandPtr));
    devTypeToCommands[0][0] = echo;
    devTypeToCommands[0][1] = beacon;
}

//Picks a command based on opcode and executes it
int executeCommand(uint32_t devType, uint32_t opCode, unsigned char isReservedCmd, unsigned char *argsData, uint32_t argsLen){
    printf("Executing command with opcode: %i\n",opCode);
    printf("Args len : %d\n",argsLen);
    if(isReservedCmd>0){
        devTypeToCommands[0][opCode](argsData, argsLen);
    }else{
        if(devType==0){
            printf("DevType not set. TERMINATING...\n");
            exit(1);
            return -1;
        }
        devTypeToCommands[devType][opCode](argsData, argsLen);
    }
}

//COMMANDS (INBOUND, FROM NODE TO SERVER)
int echo(unsigned char *args, uint32_t argsLen){
    //print recieved string
    printf("Echo: ");
    for(int i = 0; i<argsLen; i++){
        printf("%c",*(args+i));
    }
    printf("\n");
}

int beacon(unsigned char *args, uint32_t argsLen){
    //Packet sent at regular intervals from every device (After which they either send recv or both and then go to sleep and repeat)
    printf("Recieved beacon packet!\n");
}
