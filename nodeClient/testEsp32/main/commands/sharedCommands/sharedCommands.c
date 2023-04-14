#include "sharedCommands.h"

#define ANSI_COLOR_GREENBLUE "\x1B[72m"
#define ANSI_COLOR_RESET "\x1b[0m"

commandPtr *nodeCommands; // An array of functions where indexes lower than 129 corresponds to reserved opcodes and anything above that are opcodes specific to a device

// Initializes the shared node commands and allocates enough space for the amount of user commands that are needed passed as an argument
void initSharedNodeCommands(int maxUserCommands)
{
    nodeCommands = malloc(maxUserCommands * sizeof(commandPtr) + nodeToServerReservedOpCodesUpperLimit * sizeof(commandPtr));
    nodeCommands[0] = poweroffMessage;
    nodeCommands[1] = serverToNodeMessage;
    nodeCommands[2] = rebootMessage;
}

int poweroffMessage(unsigned char *args, uint32_t argsLen)
{
    printf2("Poweroff command recieved\n");
    // TODO: Not sure if this is necessary, keep as part of opcode spec
    return 0;
}

int rebootMessage(unsigned char *args, uint32_t argsLen)
{
    printf2("Reboot command recieved\n");
#if targetPlatform == 2
    esp_restart();
#endif
    return 0;
}

int serverToNodeMessage(unsigned char *args, uint32_t argsLen)
{
    printf2("Message command recieved; Message: ");
    for (int i = 0; i < argsLen; i++)
    {
        printf("%c", *(args + i));
    }
    printf("\n");
    return 0;
}

// Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...)
{
    int result;
    va_list args;
    va_start(args, formattedInput);
    printf(ANSI_COLOR_GREENBLUE "sharedCommand: " ANSI_COLOR_RESET);
    result = vprintf(formattedInput, args);
    va_end(args);
    return result;
}
