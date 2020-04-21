//Contains all the commands corresponding to all devTypes
//devType=0 is server
//devType=1 is lock

#include "commands.h"

//DevType to its opCodes dictionary
typedef int (*commandPtr)(unsigned char *args, uint32_t argsLen);  //Command pointers
commandPtr **devTypeToCommands;
commandPtr *reservedNodeCommands;

//Initializes opCodes for the specified devType, as well as the reserved ones
int initOpCodes(uint32_t devType){
    printf("Initializing op codes\n");
    devTypeToCommands = malloc(maxDevTypes*sizeof(commandPtr));
    //devTypeToCommands[0] = malloc(sizeof(commandPtr)*maxCodeLockCommands);
    //devTypeToCommands[1] = malloc(sizeof(commandPtr)*maxCodeLockCommands);

    for(int i = 0; i <maxDevTypes; i++){
        devTypeToCommands[i] = NULL;
    }
    //Commands get allocated only on the devId where they are used
    if(devType==0){
        //SERVER COMMANDS
        devTypeToCommands[0] = malloc(maxServerCommands*sizeof(commandPtr));
        devTypeToCommands[0][0] = echo;
        devTypeToCommands[0][1] = beacon;
    }


    //Always initialize reserved opcodes if the device isnt the server (i.e. for every node)
    if(devType!=0){
        reservedNodeCommands = malloc(maxNodeReservedCommands*sizeof(commandPtr));
        reservedNodeCommands[0] = poweroff;
    }

    return 0;
}


//Picks a command based on opcode and executes it
int executeCommand(uint32_t devType, uint32_t opCode, unsigned char isReservedCmd, unsigned char *argsData, uint32_t argsLen){
    printf("Executing command with opcode: %i\n",opCode);
    printf("Args len : %d\n",argsLen);
    if(isReservedCmd!=0){
        //Execute reserved node command
        reservedNodeCommands[opCode](argsData, argsLen);
    }else{
        if(devType==0){
            printf("devType set to server. Executing corresponding command.");
        }
        devTypeToCommands[devType][opCode](argsData, argsLen);
    }

    return 0;
}

//SERVER COMMANDS---------------------------------------------------------------------------
int echo(unsigned char *args, uint32_t argsLen){
    //print recieved string
    printf("Echo: ");
    for(int i = 0; i<argsLen; i++){
        printf("%c",*(args+i));
    }
    printf("\n");
    return 0;
}

int beacon(unsigned char *args, uint32_t argsLen){
    //Packet sent at regular intervals from every device (After which they either send recv or both and then go to sleep and repeat)
    printf("Recieved beacon packet!\n");

    return 0;
}


//RESERVED NODE COMMANDS--------------------------------------------------------------------
int poweroff(unsigned char *args, uint32_t argsLen){
    printf("Poweroff command executed...");
    
    return 0;
}
