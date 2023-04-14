#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/random.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

// CUSTOM
#include "commands.h"
#include "gcm.h"
#include "arrayList.h"
#include "aes-gcm.h"
#include "pckData.h"
#include "generalSettings.h"
#include "server.h"
#include "epollRecievingThread.h"
#include "broadcastThread.h"
#include "ecdh.h"
#include "webSocketThread.h"
#include "json-c/json.h"

#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_BLUE "\x1B[34m"
#define ANSI_COLOR_RESET "\x1b[0m"

// MULTITHREADING
pthread_t epollRecievingThreadID, broadcastThreadID; // Thread that listens for node and user commands
pthread_t websocketThreadID;                         // Thread that initializes a websocket client to listen for user commands
pthread_mutex_t commandsQueueMux;

arrayList connectionsInfo;           // Store info for every connection to server
arrayList recvHolders;               // Necesarry for storing recieved data sent in multiple packets. Used in a more advanced implementation of recv() function.
arrayList devInfos;                  // Stores information for every device known to the server
arrayList commandsQueue;             // Stores the commands recieved from a client(used not node). Queued up for execution
globalSettingsStruct globalSettings; // global settings. Loaded and saved in a config plaintext file
localSettingsStruct localSettings;

unsigned char decryptedMsgBuffer[MAXMSGLEN];
unsigned char sendProcessingBuffer[MAXMSGLEN];
mbedtls_gcm_context gcmCtx;

char *serverConfigFilename = "serverConfig.conf";
char *deviceConfigFilename = "deviceInfos.json";

int main(int argc, char *argv[])
{
    // Helper. Remove later.
    uint32_t testNumber = 6;
    printf2("Endianess of number 6(uint32_t). Local: ");
    for (int i = 0; i < 4; i++)
    {
        printf("%d ", *((unsigned char *)&testNumber + i));
    }
    printf(" Network: ");
    testNumber = htonl(testNumber);
    for (int i = 0; i < 4; i++)
    {
        printf("%d ", *((unsigned char *)&testNumber + i));
    }
    printf("\n");

    // INITIALIZATION
    printf2("Initializing main server\n");
    // modifySetting("passExchangeMethod", 18, 0);
    initServer();

    devInfo devInfoTest;
    memcpy(devInfoTest.devId, "TestTestTestTest", DEVIDLEN);
    memcpy(devInfoTest.key, "TestTestTestKey2", KEYLEN);
    saveDevInfoToConfigFile(&devInfoTest);

    // Test stuff
    /* devInfo testDevInfo; */
    /* memcpy(testDevInfo.devId,"TestTestTest1234",DEVIDLEN); */
    /* memcpy(testDevInfo.key,"KeyKeyKeyKey1234",KEYLEN); */
    /* addToList(&devInfos,testDevInfo.devId); */

    // Temporary password change command testing
    // commandRequestCommand_t testCommand;
    // unsigned char testCode[8] = "TestCode";
    // testCommand.opCode = 129; // Change lock code
    // testCommand.gSettings = 0;
    // testCommand.argsPtr = testCode;
    // testCommand.argsLen = 8;
    // testCommand.addPtr = NULL;
    // testCommand.addLen = 0;
    // addToCommandQueue("TestTestTest1234", &testCommand);

    // // Temporary reboot command testing
    // commandRequestCommand_t testCommand2;
    // // unsigned char testCode[8] = "TestCode";
    // testCommand2.opCode = 2; // Change lock code
    // testCommand2.gSettings = 0;
    // // testCommand.argsPtr = testCode;
    // testCommand2.argsLen = 0;
    // testCommand2.addPtr = NULL;
    // testCommand2.addLen = 0;
    // addToCommandQueue("TestTestTest1234", &testCommand2);

    pthread_create(&broadcastThreadID, NULL, broadcastThread, NULL);
    pthread_create(&epollRecievingThreadID, NULL, epollRecievingThread, NULL);
    pthread_create(&websocketThreadID, NULL, websocketThread, NULL);
    pthread_join(epollRecievingThreadID, NULL);
    pthread_join(broadcastThreadID, NULL);
}

void initServer()
{
    // Temporarily initialize local settings
    memset(localSettings.username, 0, 30);
    memset(localSettings.propertyId, 0, 24);
    memset(localSettings.propertyKey, 0, 12);
    strcpy(localSettings.username, "test@gmail.com");
    strcpy(localSettings.propertyId, "6238ca120382660655217a84");
    strcpy(localSettings.propertyKey, "propKey1");

    initBasicServerData(&connectionsInfo, &recvHolders, &devInfos, &commandsQueue, &globalSettings, 0);
    loadInServerSettings();
    initServerCommandsList();
}

void deinitServer()
{
}

// Processes a message, it can be any kind of message(node msg or user msg)
int processMsg(connInfo_t *connInfo, unsigned char *msg)
{
    printf2("Processing new message from socket: %d\n", connInfo->socket);
    // If caught beacon packet. Send it to the list of unverified devices, waiting for the user to accept/reject it. No persistent storage, just during runtime. Allow the option of instantly accepting all devices for debugging purposes.

    // MESSAGE STRUCTURE
    uint32_t msgLen = *(uint32_t *)msg;
    msgLen = ntohl(msgLen);
    uint32_t addMsgLen = *(uint32_t *)(msg + 4);
    addMsgLen = ntohl(addMsgLen);
    uint32_t extraMsgLen = *(uint32_t *)(msg + 8);
    extraMsgLen = ntohl(extraMsgLen);

    // Pointers to sections of the message
    unsigned char *msgLenPtr = msg;
    unsigned char *addLenPtr = msgLenPtr + 4;
    unsigned char *extraLenPtr = addLenPtr + 4;
    unsigned char *ivPtr = extraLenPtr + 4;
    unsigned char *tagPtr = ivPtr + IVLEN;
    unsigned char *extraDataPtr = tagPtr + TAGLEN;
    unsigned char *extraDataPckPtr = extraDataPtr + protocolSpecificExtraDataLen;
    unsigned char *addDataPtr = extraDataPtr + extraMsgLen;
    unsigned char *addPckDataPtr = addDataPtr + protocolSpecificAddLen;
    unsigned char *encryptedDataPtr = addDataPtr + addMsgLen;
    unsigned char *encryptedPckDataPtr = encryptedDataPtr + protocolSpecificEncryptedDataLen;

    /* printf2("Message lengths - TotalLen: %d; AddLen: %d; ExtraLen: %d\n",msgLen,addMsgLen,extraMsgLen); */
    printMessage("Message structure(encrypted):", msg, 0, true, true);

    // Extract the devID and hence its corresponding decryption key
    if (connInfo->connectionType == 0)
    {
        // Connection to node
        printf2("Processing a node message...\n");
        unsigned char devIdFromMessage[DEVIDLEN];
        uint8_t devType = 0;
        uint8_t connectionType = 0;

        devInfo *devInfoRef;
        pckDataToHostOrder(addPckDataPtr);
        if (getElementFromPckData(addPckDataPtr, devIdFromMessage, 0) == -1)
        {
            printf2("DEVID not present in ADD data, so ignore this message, since without DEVID we cant decrypt\n");
            return -1;
        }
        if (getElementFromPckData(addPckDataPtr,&devType,1) == -1){
            printf2("Coudlnt find devtype in ADD data\n");
        }
        if (getElementFromPckData(addPckDataPtr, &connectionType,2) == -1){
            printf2("Couldnt find connectionType in ADD data\n");
        }
        pckDataToNetworkOrder(addPckDataPtr);

        // print2("DEVID FROM ADD DATA",devIdFromMessage,DEVIDLEN,0);

        int deviceInfoFound = 0;
        for (int i = 0; i < devInfos.length; i++)
        {
            devInfoRef = getFromList(&devInfos, i);
            // printf("DEVINFO FOUND: %s\n",newDevInfo->devId);
            // printf("DEVINFO SENT: %s\n",devId);
            if (memcmp(devInfoRef->devId, devIdFromMessage, DEVIDLEN) == 0)
            {
                // Found the devID! Get key and dev type
                deviceInfoFound = 1;
                break; // So that the devInfo entry remains assigned
                // printf2("Found corresponding DEVID for the recieved message. Obtained corresponding decryption key\n");
            }
        }
        if (deviceInfoFound == 0)
        {
            printf2("No device with corresponding DEVID is known to server... Attempting to extract diffie helmann password and set up the encryption key\n");

            // Get the key sent from node
            uint8_t privateDHKey[ECC_PRV_KEY_SIZE];
            uint8_t publicDHKeyLocal[ECC_PUB_KEY_SIZE];
            uint8_t publicDHKeyRemote[ECC_PUB_KEY_SIZE];
            uint8_t sharedDHKey[ECC_PUB_KEY_SIZE];
            pckDataToHostOrder(extraDataPtr);
            getElementFromPckData(extraDataPckPtr, publicDHKeyRemote, 0);
            pckDataToNetworkOrder(extraDataPckPtr);
            print2("Public DH Key recieved: ", publicDHKeyRemote, ECC_PUB_KEY_SIZE, 0);

            // Generate public/private DH Keys, send off the public one to node
            getrandom(privateDHKey, ECC_PRV_KEY_SIZE, 0);
            ecdh_generate_keys(publicDHKeyLocal, privateDHKey);


            // Generate shared secret Key and
            ecdh_shared_secret(privateDHKey, publicDHKeyRemote, sharedDHKey);
            print2("Generated shared key: ", sharedDHKey, ECC_PUB_KEY_SIZE, 0);

            devInfo newDevInfo;
            memcpy(newDevInfo.devId, devIdFromMessage, DEVIDLEN);
            memcpy(newDevInfo.key, sharedDHKey, KEYLEN);
            newDevInfo.devType = devType;
            newDevInfo.deviceConnectionType = connectionType;

            // Save persistent data (mainly devId and encryption keys to devInfos.json file)
            saveDevInfoToConfigFile(&newDevInfo);

            //Here add non-persistent initialize settings for devInfo
            time_t currentTime;
            time(&currentTime);
            newDevInfo.lastConnectionTime = currentTime;

            // Add this devInfo to runtime variable to be used by subsequent connections
            addToList(&devInfos, &newDevInfo);

            devInfoRef = &newDevInfo;

            // Send off local public key
            unsigned char *addPckDataToSend;
            initPckData(&addPckDataToSend);
            appendToPckData(&addPckDataToSend, publicDHKeyLocal, ECC_PUB_KEY_SIZE);
            encryptAndSendAll(connInfo->socket, 0, NULL, NULL, NULL, NULL, addPckDataToSend, 0, sendProcessingBuffer);
            printf2("Successfully exchanged diffie helmann key for encryption!\n");
            return 0;
        }

        printf2("Message's DEVID: %.16s, KEY: %.16s\n", devInfoRef->devId, devInfoRef->key);

        // Decrypt the message
        initGCM(&gcmCtx, devInfoRef->key, KEYLEN * 8);
        if (decryptPckData(&gcmCtx, msg, decryptedMsgBuffer) != 0)
        {
            // Failed decryption (Could be spoofed ADD (DevId for instance) or something else, discard the message)
            printf2("Failed decrypting the message. Ignoring this message\n");
            return -1;
        }
        printf2("Message succesfully decrypted\n");
        // Copy the outputBuf(decrypted part of message) into the original message buffer, that will make it possible to use our printMessage function again on the decrypted message(Only doing this to make it easier to visually debug messages)
        uint32_t msgLenBeforeEncryptedData = 4 + 4 + 4 + IVLEN + TAGLEN + extraMsgLen + addMsgLen;
        memcpy(msg + msgLenBeforeEncryptedData, decryptedMsgBuffer, msgLen - msgLenBeforeEncryptedData);
        printMessage("Message structure(decrypted):", msg, 0, false, false);

        if (connInfo->localNonce == 0 && connInfo->remoteNonce == 0)
        {
            // No nonce sent but we just recieved the other side's nonce. Hence send ours.
            // getrandom(&(connInfo->localNonce),4,0); //Generate random nonce
            // if(connInfo->localNonce==0){
            //     //Just in case the random number is 0, since 0 signifies no nonce was set
            //     connInfo->localNonce = 1;
            // }

            // Since we are recieving a message and no nonce from our side has been sent, the other side must have sent their nonce on this message, hence get it.
            uint32_t remoteNonce = *((uint32_t *)encryptedDataPtr);
            connInfo->remoteNonce = remoteNonce;

            // Send off our part of nonce to remote (node)
            printf2("Sending off local nonce to node\n");
            // Generate dummy message
            unsigned char *pckDataEncrypted = NULL;
            unsigned char *pckDataADD = NULL;
            unsigned char *pckDataExtra = NULL;

            encryptAndSendAll(connInfo->socket, 0, connInfo, &gcmCtx, pckDataEncrypted, pckDataADD, pckDataExtra, 0, sendProcessingBuffer);

            // Convert local nonce back to host order
            connInfo->localNonce = ntohl(connInfo->localNonce);

            // SET SESSION ID AFTER SENDING OFF THE NONCE, OTHERWISE THE encryptAndSendAll() function will think that sessioID was establish and misbehave!!
            connInfo->sessionId = connInfo->remoteNonce + connInfo->localNonce; // GENERATING THE NEW SESSION ID BY ADDING UP THE NONCES

            printf2("Nonce local: %u; Nonce remote: %u; Generated sessionID: %u\n", connInfo->localNonce, connInfo->remoteNonce, connInfo->sessionId);
            printf2(ANSI_COLOR_BLUE "SessionID established. SessionID: %u\n" ANSI_COLOR_RESET, connInfo->sessionId);

            // NOW SEND OFF ANY MESSAGES IN THE QUEUE AND RECIEVE ANY NEW MESSAGES OR REPLIES FROM NODE
            sendQueuedMessages(connInfo, devInfoRef);
            /* printf2("Closing the connection\n"); */
            /* close1(connInfo->socket,&connectionsInfo); */
        }
        else if (connInfo->remoteNonce == 0 && connInfo->localNonce != 0)
        {
            // We sent our nonce before but have just recieved the nonce of the other side

            uint32_t *remoteNonce = (uint32_t *)encryptedDataPtr;
            connInfo->remoteNonce = *remoteNonce;

            connInfo->sessionId = connInfo->remoteNonce + connInfo->localNonce; // GENERATING THE NEW SESSION ID BY ADDING UP THE NONCES

            printf2(ANSI_COLOR_BLUE "SessionID established\n" ANSI_COLOR_RESET);
            printf2("Nonce local: %d; Nonce remote: %d; Generated sessionID: %d\n", connInfo->localNonce, connInfo->remoteNonce, connInfo->sessionId);

            // NOW SEND OFF ANY MESSAGES IN THE QUEUE
            sendQueuedMessages(connInfo, devInfoRef);
            /* printf2("Closing the connection\n"); */
            /* close1(connInfo->socket,&connectionsInfo); */
        }
        else if (connInfo->sessionId != 0)
        {
            // Process actual messages
            printf2("PROCESSING ACTUAL MESSAGE\n");
            uint32_t devtype, opcode, argsLen;
            getElementFromPckData(encryptedPckDataPtr, (unsigned char *)&devtype, 0);
            getElementFromPckData(encryptedPckDataPtr, (unsigned char *)&opcode, 1);
            argsLen = getElementLengthFromPckData(encryptedPckDataPtr, 2);
            unsigned char args[argsLen];
            getElementFromPckData(encryptedPckDataPtr, args, 2);

            // Serialization
            devtype = ntohl(devtype);
            opcode = ntohl(opcode);
            

            printf2("devtype: %d; opcode: %d; argsLen: %d\n", devtype, opcode, argsLen);

            // Update the deviceInfo with corresponding devtype
            int deviceInfoFound = 0;
            for (int i = 0; i < devInfos.length; i++)
            {
                devInfoRef = getFromList(&devInfos, i);
                // printf("DEVINFO FOUND: %s\n",newDevInfo->devId);
                // printf("DEVINFO SENT: %s\n",devId);
                if (memcmp(devInfoRef->devId, devIdFromMessage, DEVIDLEN) == 0)
                {
                    // Found the devID! Get key and dev type
                    deviceInfoFound = 1;
                    printf2("Updating devInfo with devtype");
                    devInfoRef->devType = devtype;
                    break; // So that the devInfo entry remains assigned
                    // printf2("Found corresponding DEVID for the recieved message. Obtained corresponding decryption key\n");
                }
            }

            if (opcode <= nodeToServerReservedOpCodesUpperLimit)
            {
                // Reserved opcode henc devtype=0
                /* printf2("Executing reserved opcode\n"); */
                serverCommands[0][opcode](devIdFromMessage, (uint8_t)devtype, args, argsLen);
            }
            else
            {
                /* printf2("Executing non-reserved opcode\n"); */
                serverCommands[devtype][opcode](devIdFromMessage, (uint8_t)devtype, args, argsLen);
            }

            /* if(devtype==1){ */
            /*     //TEST DEVICE */
            /*     if(opcode==0){ */
            /*         //Beacon packet */
            /*         printf2("Beacon packet recieved. (Data:%s)\n",args); */
            /*     } */
            /* } */
        }
    }

    // Clear out message buffer
    memset(msg, 0, MAXMSGLEN);
}

// Empties the server message queue for specific device and sends them all off to corresponding device
void sendQueuedMessages(connInfo_t *connInfo, devInfo *devInfo)
{
    pthread_mutex_lock(&commandsQueueMux);
    printf2("Sending off queued messages for DEVID: %.16s\n", devInfo->devId);
    printf2("Command queue length: %d\n", commandsQueue.length);
    uint8_t messageSent = 0;
    for (int i = 0; i < commandsQueue.length; i++)
    {
        commandRequest_t *commandsRequest = getFromList(&commandsQueue, i);
        printf2("Message queue number: %d. Target DEVID: %.16s\n", i, commandsRequest->devId);
        /* printf2("our DEVID: %s. Stored DEVID: %s\n",commandsRequest->devId,devInfo->devId); */
        if ((memcmp(commandsRequest->devId, devInfo->devId, DEVIDLEN)) == 0)
        {
            // Found corresponding commandsRequest to our DEVID
            arrayList *commandsList = &(commandsRequest->commandRequestsCommands);
            for (int i = 0; i < commandsList->length; i++)
            {
                // Send off all commands from it
                commandRequestCommand_t *command = getFromList(commandsList, i);
                sendOffCommand(command, connInfo, devInfo->devId);
            }

            // Now remove these messages from queue after sending them
            removeFromList(&commandsQueue, i);
        }
    }
    pthread_mutex_unlock(&commandsQueueMux);
}

void addToCommandQueue(unsigned char *DEVID, commandRequestCommand_t *command)
{

    printf2("Adding to command queue\n");

    pthread_mutex_lock(&commandsQueueMux);
    for (int i = 0; i < commandsQueue.length; i++)
    {
        commandRequest_t *commandRequest = getFromList(&commandsQueue, i);
        if ((memcmp(commandRequest->devId, DEVID, DEVIDLEN)) == 0)
        {
            // Add to existing commandRequest
            addToList(&(commandRequest->commandRequestsCommands), command);
        }
    }

    // If no corresponding commandRequest for the DEVID was found. Create a new one and add the command
    commandRequest_t newCommandRequest;
    commandRequest_t *newCommandRequestRef = NULL;
    addToList(&commandsQueue, &newCommandRequest);
    newCommandRequestRef = getFromList(&commandsQueue, commandsQueue.length - 1);
    memcpy(newCommandRequestRef->devId, DEVID, DEVIDLEN);
    arrayList newCommandList;
    initList(&newCommandList, sizeof(commandRequestCommand_t));
    newCommandRequestRef->commandRequestsCommands = newCommandList;
    addToList(&(newCommandRequestRef->commandRequestsCommands), command);

    pthread_mutex_unlock(&commandsQueueMux);
}

void sendOffCommand(commandRequestCommand_t *command, connInfo_t *connInfo, unsigned char *devId)
{
    printf2("Sending off command with opcode: %d\n", command->opCode);
    unsigned char *encryptedPckDataPtr = NULL;
    // unsigned char *addPckDataPtr = command->addPtr;
    unsigned char *addPckDataPtr = NULL;
    unsigned char *extraPckDataPtr = NULL;

    if (command->addPtr != NULL)
    {
        initPckData(&addPckDataPtr);
        appendToPckData(&addPckDataPtr, command->addPtr, command->addLen);
    }

    initPckData(&encryptedPckDataPtr);
    appendToPckData(&encryptedPckDataPtr, (unsigned char *)&(command->opCode), sizeof(uint32_t));
    if (command->argsPtr != NULL)
    {
        appendToPckData(&encryptedPckDataPtr, command->argsPtr, command->argsLen);
    }
    unsigned char *key = findEncryptionKey(devId);
    initGCM(&gcmCtx, key, KEYLEN * 8);
    encryptAndSendAll(connInfo->socket, 0, connInfo, &gcmCtx, encryptedPckDataPtr, addPckDataPtr, extraPckDataPtr, command->gSettings, sendProcessingBuffer);
}

// Returns the encryption key for the corresponding DEVID by searching through devInfos arrayList
unsigned char *findEncryptionKey(unsigned char *DEVID)
{
    for (int i = 0; i < devInfos.length; i++)
    {
        devInfo *devInfo = getFromList(&devInfos, i);
        if ((memcmp(devInfo->devId, DEVID, DEVIDLEN)) == 0)
        {
            // Found the key
            return devInfo->key;
        }
    }
    return NULL; // No key found
}

// Adds a new or modifies an existing devInfos data entry in deviceInfos.json config file
int saveDevInfoToConfigFile(devInfo *data)
{

    printf2("Saving devInfo to devInfos.json config file\n");
    // Get the json config file
    json_object *configFile = json_object_from_file(deviceConfigFilename);

    printf2("JSON FILE BEFORE MODIFYING\n");
    printf2("The json file:\n\n%s\n", json_object_to_json_string_ext(configFile, JSON_C_TO_STRING_PRETTY));

    if (!configFile)
    {
        // File cant be opened
        printf2("Failed opening device config file\n");
    }
    else
    {
        // File opened
        printf2("Opened device config file\n");

        json_object *devInfosArray;

        // See if devInfos array exists
        if (json_object_object_get_ex(configFile, "deviceInfos", &devInfosArray))
        {
            // Exists
            printf2("Found existing deviceInfos array in device config file. Adding to it\n");
        }
        else
        {
            // Doenst exist - create a new one
            printf2("Could not find existing deviceInfos array in device config file. Creating a new one\n");
            json_object *newDevInfosArray;
            newDevInfosArray = json_object_new_array();
            json_object_object_add(configFile, "deviceInfos", newDevInfosArray);
            devInfosArray = newDevInfosArray;
        }

        // See if this data entry already exists, if so update the data inside that entry TODO:
        int arrayLen = json_object_array_length(devInfosArray);
        int dataEntryFound = 0;
        json_object *temp;
        for (int i = 0; i < arrayLen; i++)
        {
            temp = json_object_array_get_idx(devInfosArray, i);
            json_object *devId = json_object_object_get(temp, "devId");

            // See if devId matches
            if (memcmp(data->devId, json_object_get_string(devId), DEVIDLEN) == 0)
            {
                printf2("Found existing data entry for this device. Modifying it\n");
                dataEntryFound = 1;
                
                //Change encryption key
                json_object *encryptionKey = json_object_object_get(temp,"encryptionKey");
                json_object_set_string_len(encryptionKey,data->key,KEYLEN);

                //Change devtype
                json_object *devtype = json_object_object_get(temp,"devtype");
                json_object_set_int(devtype,data->devType);

                //Change connection type
                json_object *deviceConnectionType = json_object_object_get(temp,"deviceConnectionType");
                json_object_set_int(devtype,data->deviceConnectionType);
            }
        }

        // Add new data entry if existing entry wasnt already modified
        if (dataEntryFound == 0)
        {
            json_object *newDataEntry;
            newDataEntry = json_object_new_object();
            json_object_object_add(newDataEntry, "devId", json_object_new_string_len(data->devId, DEVIDLEN));
            json_object_object_add(newDataEntry, "encryptionKey", json_object_new_string_len(data->key, KEYLEN));
            json_object_object_add(newDataEntry, "devtype",json_object_new_int(data->devType));
            json_object_object_add(newDataEntry, "deviceConnectionType",json_object_new_int(data->deviceConnectionType));
            json_object_array_add(devInfosArray, newDataEntry);
        }

        printf2("JSON FILE AFTER MODIFYING\n");
        printf2("The json file:\n\n%s\n", json_object_to_json_string_ext(configFile, JSON_C_TO_STRING_PRETTY));

        // Write to file the updated version
        json_object_to_file(deviceConfigFilename, configFile);

        // Free up memory
        json_object_put(configFile);
    }
}

int loadInServerSettings()
{

    // Load in device config file
    printf2("Opening device config file\n");
    json_object *configFile = json_object_from_file(deviceConfigFilename);

    if (!configFile)
    {
        // File cant be opened
        printf2("Failed opening device config file\n");
    }
    else
    {
        // File opened
        printf2("Opened device config file\n");
        printf2("The json file:\n\n%s\n", json_object_to_json_string_ext(configFile, JSON_C_TO_STRING_PRETTY));

        // Json file contains one key (deviceInfos) which is an array of objects,
        // within the array each object is device info for specific node device.
        // Get each data entry and save to devInfos arrayList for runtime

        json_object *devInfosArray = json_object_object_get(configFile, "deviceInfos");
        if (devInfosArray != NULL)
        {
            json_object *temp;
            int arrayLen = json_object_array_length(devInfosArray);

            for (int i = 0; i < arrayLen; i++)
            {
                temp = json_object_array_get_idx(devInfosArray, i);
                json_object *devId = json_object_object_get(temp, "devId");
                json_object *encryptionKey = json_object_object_get(temp, "encryptionKey");
                json_object *devtype = json_object_object_get(temp,"devtype");
                json_object *deviceConnectionType = json_object_object_get(temp,"deviceConnectionType");

                // Generate new devInfo and append to devInfos array
                printf("Read devInfo for - devId: %s; encryptionKey: %s; Adding to devInfos array\n", json_object_get_string(devId), json_object_get_string(encryptionKey));
                devInfo newDevInfo;
                memcpy(newDevInfo.devId, json_object_get_string(devId), DEVIDLEN);
                memcpy(newDevInfo.key, json_object_get_string(encryptionKey), KEYLEN);
                newDevInfo.devType = json_object_get_int(devtype);
                newDevInfo.deviceConnectionType = json_object_get_int(deviceConnectionType);

                addToList(&devInfos, &newDevInfo);
            }
        }
        else
        {
            // No devInfosArray present in object
            printf2("No devInfos array in the json config file\n");
        }

        // Free up the memory after finished reading
        json_object_put(configFile);
    }

    // Load in server config file
    printf2("Loading server settings config file\n");
    FILE *settingsFile;
    char temp[60];
    int currentCharacters;

    if ((settingsFile = fopen(serverConfigFilename, "r")) == NULL)
    {
        printf2("Failed opening server settings file\n");
        return -1;
    }

    // Analyze every line
    while (fgets(temp, 60, settingsFile) != NULL)
    {
        currentCharacters = strcspn(temp, "\n");
        char *settingName = 0;
        char *settingValue = 0;
        char *settingNameBegin = 0;
        int settingsNamePassedFirstQuote = 0;
        int settingNameComplete = 0;
        char *settingValueBegin = 0;
        int settingValuePassedFirstQuote = 0;
        int settingValueComplete = 0;

        printf2("Loading in server settings...\n");

        for (int i = 0; i < currentCharacters; i++)
        {
            if (*(temp + i) == '"' && settingsNamePassedFirstQuote == 1 && settingNameComplete != 1)
            {
                // Malloc space for setting name (and later value), remember to dealloc
                int settingNameLen = (temp + i) - settingNameBegin - 1;
                printf2("CALLING MALLOC\n");
                settingName = malloc(settingNameLen);
                memcpy(settingName, settingNameBegin + 1, settingNameLen);
                *(settingName + settingNameLen) = 0;
                settingNameComplete = 1;
            }
            else if (*(temp + i) == '"' && settingsNamePassedFirstQuote == 0 && settingNameComplete != 1)
            {
                settingNameBegin = temp + i;
                settingsNamePassedFirstQuote = 1;
            }
            else if (*(temp + i) == '"' && settingValuePassedFirstQuote == 1 && settingNameComplete == 1)
            {
                int settingValueLen = (temp + i) - settingValueBegin - 1;
                printf2("CALLING MALLOC\n");
                settingValue = malloc(settingValueLen);
                memcpy(settingValue, settingValueBegin + 1, settingValueLen);
                *(settingValue + settingValueLen) = 0;
                settingValueComplete = 1;
                // printf("settings value len: %d\n",settingValueLen);
                // print2("setting original",settingValueBegin+1,DEVIDLEN,0);
                // print2("setting copied",settingValue,DEVIDLEN,0);
            }
            else if (*(temp + i) == '"' && settingValuePassedFirstQuote == 0 && settingNameComplete == 1)
            {
                settingValueBegin = temp + i;
                settingValuePassedFirstQuote = 1;
            }

            if (settingValueComplete == 1)
            {
                break;
            }
        }
        printf2("NAME: %s\n", settingName);
        printf2("VALUE: %s\n", settingValue);
        //     //Analyze the obtained setting name and values and dealloc
        //     if (strcmp(settingName, "devId") == 0)
        //     {
        //         memcpy(localNodeSettings.devId, settingValue, DEVIDLEN);
        //     }
        //     else if (strcmp(settingName, "devType") == 0)
        //     {
        //         localNodeSettings.devType = atoi(settingValue);
        //     }
        //     printf2("CALLING FREE\n");
        //     free(settingName);
        //     free(settingValue);
    }
    fclose(settingsFile);
    // printf2("Finished reading node settings\n");
    return 1;
    // #elif targetPlatform == 2
    // printf2("Opening node settings file using esp32's filesystem\n");

    // return 1;
    // #endif
}

// Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...)
{
    int result;
    va_list args;
    va_start(args, formattedInput);
    printf(ANSI_COLOR_GREEN "mainServer: " ANSI_COLOR_RESET);
    result = vprintf(formattedInput, args);
    va_end(args);
    return result;
}
