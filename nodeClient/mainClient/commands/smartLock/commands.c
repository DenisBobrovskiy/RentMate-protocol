
#include "commands.h"

#define ANSI_COLOR_LIGHTPURPLE  "\x1B[141m"
#define ANSI_COLOR_RESET "\x1b[0m"

void initNodeCommands(){
    //Init the shared commands (also allocates the space for the user commands)
    initSharedNodeCommands(maxCodeLockCommands);
    nodeCommands[129] = changePasscode;
}

int changePasscode(unsigned char *args, uint32_t argsLen){
    printf2("Change passcode command recieved! New passcode: ");
    for(int i = 0; i < argsLen; i++){
        printf("%c",*(args+i));
    }
    printf("\n");
    return 0;
}

//Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...){
    int result;
    va_list args;
    va_start(args,formattedInput);
    printf(ANSI_COLOR_LIGHTPURPLE "nodeCommands: " ANSI_COLOR_RESET);
    result = vprintf(formattedInput,args);
    va_end(args);
    return result;
}
