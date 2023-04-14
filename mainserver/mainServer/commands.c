#include "pckData.h"
#include <stdio.h>
#include "server.h"
#include "commands.h"

#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

commandPtr **serverCommands; // A 2d array of functions where index corresponds to the devtype and second index corresponds to opcode

void initServerCommandsList()
{
    printf2("Initialized server commands list\n");
    serverCommands = malloc(maxDevTypes * sizeof(commandPtr));
    serverCommands[0] = malloc(nodeToServerReservedOpCodesUpperLimit * sizeof(commandPtr));
    serverCommands[1] = malloc(maxTestDeviceCommands * sizeof(commandPtr));

    serverCommands[0][0] = nodeToServerBeacon;
    serverCommands[0][1] = nodeToServerMessage;
}

void deinitServerCommandsList()
{
    if (serverCommands != NULL)
    {
        free(serverCommands);
    }
}

void updateDeviceStatus(devInfo *devInfoRef){
    //Update connection time
    time_t currentTime;
    struct tm *timeinfo;
    time(&currentTime);
    timeinfo = localtime(&currentTime);

    devInfoRef->lastConnectionTime = currentTime;
    printf2( "Current local time and date: %s\n", asctime (timeinfo) );
}

// RESERVED OPCODE COMMANDS

int nodeToServerBeacon(unsigned char *devid, uint8_t devtype, unsigned char *args, uint32_t argsLen)
{
    devInfo *devInfoRefMain = NULL;
    printf2("Beacon command message recieved. Length: %d\n", argsLen);
    for (int i = 0; i < argsLen; i++)
    {
        printf("%c", *(args + i));
    }
    printf("\n");

    if (devtype == 2)
    {
        // Smart lock beacon.
        printf2("Beacon from smart lock recieved\n");
        uint8_t lockStatus, doorStatus, passwordLen;
        uint8_t password[12];
        memcpy(&lockStatus, args, 1);
        memcpy(&doorStatus, args + 1, 1);
        memcpy(&passwordLen, args + 2, 1);
        memcpy(&password, args + 3, passwordLen);

        printf2("Lock status: %d\n", lockStatus);
        printf2("Door status: %d\n", doorStatus);
        printf2("Password lenght: %d\n", passwordLen);
        printf2("Password: ");
        for (int i = 0; i < passwordLen; i++)
        {
            printf("%d", password[i]);
        }
        printf("\n");

        // Find corresponding devInfo and assign these values into it

        int deviceInfoFound = 0;
        devInfo *devInfoRef;
        for (int i = 0; i < devInfos.length; i++)
        {
            devInfoRef = getFromList(&devInfos, i);
            // printf("DEVINFO FOUND: %s\n",newDevInfo->devId);
            // printf("DEVINFO SENT: %s\n",devId);
            if (memcmp(devInfoRef->devId, devid, DEVIDLEN) == 0)
            {
                // Found the devID! Get key and dev type
                deviceInfoFound = 1;
                printf2("FOUND CORRESPONDING DEVINFO IN BEACON COMMAND");
                devInfoRefMain = devInfoRef;
                devInfoRef->lockState = lockStatus;
                devInfoRef->doorState = doorStatus;
                devInfoRef->passwordLen = passwordLen;
                memcpy(devInfoRef->password,password,passwordLen);
                break; // So that the devInfo entry remains assigned
                // printf2("Found corresponding DEVID for the recieved message. Obtained corresponding decryption key\n");
            }
        }
    }
    if(devtype==5){
        printf2("Recieved beacon packet from noise monitor\n");
        uint8_t noiseLevel = *args;

        int deviceInfoFound = 0;
        printf2("NOISE LEVEL: %d\n",noiseLevel);
        devInfo *devInfoRef;
        for (int i = 0; i < devInfos.length; i++)
        {
            devInfoRef = getFromList(&devInfos, i);
            // printf("DEVINFO FOUND: %s\n",newDevInfo->devId);
            // printf("DEVINFO SENT: %s\n",devId);
            if (memcmp(devInfoRef->devId, devid, DEVIDLEN) == 0)
            {
                // Found the devID! Get key and dev type
                deviceInfoFound = 1;
                printf2("FOUND CORRESPONDING DEVINFO IN BEACON COMMAND\n");
                devInfoRefMain = devInfoRef;
                devInfoRef->noiseLevel = noiseLevel;
                // memcpy(devInfoRef->password,password,passwordLen);
                break; // So that the devInfo entry remains assigned
                // printf2("Found corresponding DEVID for the recieved message. Obtained corresponding decryption key\n");
            }
        }
    }

    //Update status if device info was found
    if(devInfoRefMain!=NULL){
        updateDeviceStatus(devInfoRefMain);
    }
    return 0;
}

int nodeToServerMessage(unsigned char *devid, uint8_t devtype, unsigned char *args, uint32_t argsLen)
{
    printf2("Message command recieved; Message: ");
    for (int i = 0; i < argsLen; i++)
    {
        printf("%c", *(args + i));
    }
    printf2("\n");
    return 0;
}

// Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...)
{
    int result;
    va_list args;
    va_start(args, formattedInput);
    printf(ANSI_COLOR_GREEN "mainServer(commands.c): " ANSI_COLOR_RESET);
    result = vprintf(formattedInput, args);
    va_end(args);
    return result;
}
