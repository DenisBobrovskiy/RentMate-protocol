// Shared includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <time.h>
#include <memory.h>
#include <string.h>
#include "client.h"
#include "socketThread.h"
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include "compileFlag.h"
#include "client.h"
#include "math.h"

// Custom library includes
#if targetPlatform == 2
#include "aes-gcm.h"
#include "arrayList.h"
#include "pckData.h"
#include "ecdh.h"
#elif targetPlatform == 1
#include "arrayList.h"
#include "pckData.h"
#include "generalSettings.h"
#include "ecdh.h"
#endif

// ESP-IDF includes
#if targetPlatform == 2
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "pckData.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_pthread.h"
#include "esp_heap_trace.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_spiffs.h"
#endif

// Device specific includes
#include "./commands/sharedCommands/sharedCommands.h"
#if targetDevtype == 2 // Smart lock
#include "./commands/smartLock/commands.h"
#include "./smartLock/matrixKeyboard.h"
#include "./smartLock/smartLockCore.h"

#elif targetDevtype == 3 // Smart socket
#include "./commands/smartSocket/commands.h"

#elif targetDevtype == 4 // Presence Detector
#include "./commands/presenceDetector/commands.h"

#elif targetDevtype == 5 // Noise monitor
#include "./commands/noiseMonitor/commands.h"
#include "./noiseMonitor/noiseMonitorCore.h"

#endif

#define ANSI_COLOR_BLUE "\x1B[34m"
#define ANSI_COLOR_WHITE "\x1B[37m"

// WIFI DATA

// #define networkSSID "w"
// #define networkPassword "IforgotThePa$$w0rd"

#define networkSSID "testbench"
#define networkPassword "testbenchPassword"

// #define networkSSID "TP-Link_1A84"
// #define networkPassword "38794261"

// #define networkSSID "TP-Link_E69940"
// #define networkPassword "73594536"

// #define networkSSID "casamoner WIFI-5G"
// #define networkPassword "kamuteco"

#define wifiMaximumReconnectAttempts 100

// Devtype specific values (DONT PUT ANY HERE UNLESS NECESARRY, STORE DEVICE SPECIFIC VARIABLES IN THEIR SOURCE FILES)
#if targetDevtype == 2 // Smart lock

#elif targetDevtype == 3 // Smart socket

#elif targetDevtype == 4 // Presence detector

#elif targetDevtype == 5 // Noise monitor

#endif

#if targetPlatform == 1
char *nodeSettingsFileName = "nodeSettings.conf";
#elif targetPlatform == 2

// WIFI SETTINGS for esp32
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
static const char *TAG = "wifi station";
static int s_retry_num = 0;

// Configuration file settings
char *nodeSettingsFileName = "/spiffs/nodeSettings.conf";

#endif

// Static function prototypes
#if targetPlatform == 2
static void wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
#endif

nodeSettings_t localNodeSettings;
globalSettingsStruct globalSettings;
// connInfo_t connInfo;
// unsigned char sendProcessingBuffer[MAXMSGLEN];
pthread_t socketThreadID;
unsigned char decryptionBuffer[MAXMSGLEN];
mbedtls_gcm_context encryptionContext;
int hasEncryptionContextBeenEstablished = 0;

arrayList nodeCommandsQueue;
arrayList recvHolders;
pthread_mutex_t commandQueueAccessMux;

uint8_t publicDHKeyLocalBuffer[ECC_PUB_KEY_SIZE];
uint8_t privateDHKeyBuffer[ECC_PRV_KEY_SIZE];

unsigned char pointerToKey[16] = "";
char broadcastMessageExpected[16] = "BroadcastV100000";

uint32_t sizeOfSerializedCmdInfo = 0;

// unsigned char deviceName[20];
// #if targetPlatform ==2
// memcpy(deviceName,"TestLock",8);
// #elif targetPlatform ==3
// memcpy(deviceName,"TestLock",8);
// #endif



#if targetPlatform == 2
int app_main()
{
#elif targetPlatform == 1
int main()
{
#endif
    // Main init function for all platforms
    initClient();
    unsigned char testCode[8] = "TestCode";
    nodeCommands[1](testCode, 8);

    // Temporary hardcoded settings. TODO: Fix the file loading on esp32!



    #if targetDevtype==2
    localNodeSettings.devType = 2;
    #elif targetDevtype==3
    localNodeSettings.devType = 3;
    #elif targetDevtype==4
    localNodeSettings.devType = 4;
    #elif targetDevtype==5
    localNodeSettings.devType = 5;
    #endif

    // Initialize node command queue
    pthread_mutex_init(&commandQueueAccessMux, NULL);
    initializeCommandQueue(&nodeCommandsQueue);

    // Send test command
    nodeCmdInfo testBeaconCmdInfo;
    // composeBeaconPacketCmdInfo(&testBeaconCmdInfo, (unsigned char *)"TestBeacon1", 12);
    // addToCommandQueue(&nodeCommandsQueue, &testBeaconCmdInfo);

    printf2("Creating socket thread\n");
    // Create socket thread
    pthread_create(&socketThreadID, NULL, socketThread, NULL);

#if targetPlatform == 2
// Set pin for the motor driver EN high
//  gpio_set_direction(26,GPIO_MODE_OUTPUT);
//  gpio_set_level(26,1);

// gpio_pad_select_gpio(2);
// gpio_set_direction(2,GPIO_MODE_OUTPUT);

// //Temporary loop to test out functionality
// adc1_config_width(ADC_WIDTH_BIT_12);
// adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_0);
// int val = adc1_get_raw(ADC1_CHANNEL_0);

// Init commands specific to different devtypes
#if targetDevtype == 2
    initLock();
#elif targetDevtype == 5
    initNoiseMonitor();
#endif

    // Main loop
    while (true)
    {
        // printf2("testing\n");
        // gpio_set_level(2,1);

        // Get keypad voltages
        //  int row1Vol = gpio_get_level(KEYPAD_ROW1_PIN);
        //  int row2Vol = gpio_get_level(KEYPAD_ROW2_PIN);
        //  int row3Vol = gpio_get_level(KEYPAD_ROW3_PIN);
        //  int row4Vol = gpio_get_level(KEYPAD_ROW4_PIN);
        //  int val = adc1_get_raw(ADC1_CHANNEL_0);
        //  printf2("Keypad voltages: 1: %d, 2: %d, 3: %d, 4: %d, ADC: %d\n",row1Vol,row2Vol,row3Vol,row4Vol,val);

        #if targetDevtype == 2 //Smart lock

        sleep_ms(500);
        // int output = gpio_get_level(SWITCH_LOCK_OPEN_PIN);
        // int output2 = gpio_get_level(SWITCH_LOCK_CLOSED_PIN);
        // printf2("switch door opened state: %d\n",output);
        // printf2("switch door closed state: %d\n",output2);

        printf2("Current door state: %d. Current lock state: %d\n",currentDoorState,currentLockState);

        // //Send beacon command
        // nodeCmdInfo beaconCmdInfo;
        // if(connInfo.sessionId!=0){
        //     printf2("Sending lock beacon packet\n");
        //     composeSmartLockBeaconCmdInfo(&beaconCmdInfo,currentLockState,currentDoorState,currentPasscode,currentPasscodeLen);
        //     addToCommandQueue(&nodeCommandsQueue, &beaconCmdInfo);
        // }


        #endif

        #if targetDevtype == 4
        printf2("Presence detector running\n");
        if(connInfo.sessionId!=0){
            printf2("Sending noise beacon packet\n");
            compo(&beaconCmdInfo, valDb);
            addToCommandQueue(&nodeCommandsQueue, &beaconCmdInfo);
        }
        sleep_ms(500);
        #endif

        #if targetDevtype == 5 //Noise monitor
        printf2("Noise monitor running\n");
        int val = adc1_get_raw(ADC1_CHANNEL_4);
        float valFloat = (float)val;
        printf2("Noise level: %d\n",val);

        //Get log level
        float valDb = 20 * log(valFloat/4096.0) * -1;
        int valFloor = (int)floor(valDb);
        // float valDb = (20. * log(10)) *
        printf2("Noise level (dB): %.3f\n",valDb);
        printf2("Noise level (dB): %d\n",valFloor);

        //Send beacon command
        nodeCmdInfo beaconCmdInfo;
        if(connInfo.sessionId!=0){
            printf2("Sending noise beacon packet\n");
            composeNoiseMonitorBeaconCmdInfo(&beaconCmdInfo, valDb);
            addToCommandQueue(&nodeCommandsQueue, &beaconCmdInfo);
        }

        sleep_ms(500);
        #endif
    }

#endif

    pthread_join(socketThreadID, NULL);

    return 0;
}

char *rand_string(char *str, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK";
    #if targetPlatform==1
    if (size) {
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
    }
    #elif targetPlatform==2
    if (size) {
        for (size_t n = 0; n < size; n++) {
            int key = esp_random() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
    }
    #endif
    return str;
}


void initClient()
{
#if targetPlatform == 1
    printf2("Initializing linux client\n");
#elif targetPlatform == 2
    printf2("Initializing esp32 client\n");

    // Initialize storage filesystems
    initStorage();
    // Initialize wifi stack for esp32
    initWifiStationMode();

#endif
    // Platform agnostic init functions

    // Load in settings
    loadInNodeSettings();
    initNodeCommands();
    initBasicClientData(&recvHolders, &globalSettings, localNodeSettings.devType);
    sizeOfSerializedCmdInfo = DEVIDLEN + 4 + 4 + 4 + sizeof(unsigned char *);
}

int processMsg(connInfo_t *connInfo, unsigned char *message)
{
    printf2("Processing recieved message\n");

    // MESSAGE STRUCTURE
    uint32_t msgLen = *(uint32_t *)message;
    msgLen = ntohl(msgLen);
    uint32_t addMsgLen = *(uint32_t *)(message + 4);
    addMsgLen = ntohl(addMsgLen);
    uint32_t extraMsgLen = *(uint32_t *)(message + 8);
    extraMsgLen = ntohl(extraMsgLen);

    // Pointers to sections of the message
    unsigned char *msgLenPtr = message;
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
    printMessage("Message structure(encrypted):", message, 0, true, true);

    if (connInfo == NULL)
    {
        // No connInfo, hence assume this is a diffie helmann reply from the server
        printf2("Recieved diffie helmann public key from server\n");
        uint8_t publicDHKeyRemote[ECC_PUB_KEY_SIZE];
        uint8_t sharedDHKey[ECC_PUB_KEY_SIZE];

        // Get the remote public key from message
        pckDataToHostOrder(extraDataPckPtr);
        getElementFromPckData(extraDataPckPtr, publicDHKeyRemote, 0);
        pckDataToNetworkOrder(extraDataPckPtr);

        // Generate shared key
        ecdh_shared_secret(privateDHKeyBuffer, publicDHKeyRemote, sharedDHKey);
        print2("Shared DH Key: ", sharedDHKey, ECC_PUB_KEY_SIZE, 0);
        memcpy(pointerToKey, sharedDHKey, KEYLEN);
        initGCM(&encryptionContext, pointerToKey, KEYLEN * 8);
        saveEncryptionKeyToMemory(pointerToKey);
        hasEncryptionContextBeenEstablished = 1;

        // Write the new encryption key to device memory
        printf2("Writing new encryption key to non-volatile memory\n");
#if targetPlatform == 2
        nvs_handle_t my_handle;
        esp_err_t err = nvs_open("nvs", NVS_READWRITE, &my_handle);
        if (err != ESP_OK)
        {
            printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        }
        else
        {
            printf("Done\n");
        }
#endif

        // Save the key to be used for encryption

        return 0;
    }

    decryptPckData(&encryptionContext, message, decryptionBuffer);

    // Since before any communication the client sends a beacon packet to server. If we get a message and sessionID is yet to be established it means the server sent its part of the sessionID.
    if (connInfo->sessionId == 0)
    {
        print2("decrypted data:", decryptionBuffer, 10, 0);
        // Get the remote nonce and establish sessionID
        connInfo->remoteNonce = *((uint32_t *)decryptionBuffer);
        connInfo->localNonce = htonl(connInfo->localNonce);
        connInfo->sessionId = connInfo->localNonce + connInfo->remoteNonce;
        printf2("local nonce: %u; remote: %u\n", connInfo->localNonce, connInfo->remoteNonce);
        printf2(ANSI_COLOR_BLUE "SessionID established. SessionID: %u\n" ANSI_COLOR_WHITE, connInfo->sessionId);
        return 0;
    }
    else if (connInfo->sessionId != 0)
    {
        // We received a server side message
        printf2("Recieved a message from server.\n");
        printMessage("Recieved message(decrypted)", message, 0, false, false);

        printf2("PROCESSING ACTUAL MESSAGE\n");
        uint32_t opcode, argsLen;
        print2("Encrypted data: ", encryptedPckDataPtr, 40, 0);
        getElementFromPckData(encryptedPckDataPtr, (unsigned char *)&opcode, 0);
        argsLen = getElementLengthFromPckData(encryptedPckDataPtr, 1);
        unsigned char args[argsLen];
        getElementFromPckData(encryptedPckDataPtr, args, 1);

        // Serialization
        /* opcode = ntohl(opcode); */

        printf2("opcode: %d; argsLen: %d\n", opcode, argsLen);
        nodeCommands[opcode](args, argsLen);

        return 0;
    }
    else
    {
        printf2("ERROR. SessionID is not found for a new message. (This path is not supposed to be accessible)");
        return 1;
    }
}

// Composes a generic message (not fully formatted for the protocol, use pckData functions to complete the  message) use this for functions that compose different message types with different arguments
int composeNodeMessage(nodeCmdInfo *currentNodeCmdInfo, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd)
{
    initPckData(pckDataAdd);
    initPckData(pckDataEncrypted);

    uint32_t devType = htonl(currentNodeCmdInfo->devType);
    uint32_t opcode = htonl(currentNodeCmdInfo->opcode);

    appendToPckData(pckDataAdd, (unsigned char *)currentNodeCmdInfo->devId, DEVIDLEN);

    appendToPckData(pckDataEncrypted, (unsigned char *)&(devType), 4);
    appendToPckData(pckDataEncrypted, (unsigned char *)&(opcode), 4);
    appendToPckData(pckDataEncrypted, (unsigned char *)(currentNodeCmdInfo->args), currentNodeCmdInfo->argsLen);

    printPckData(NULL, *pckDataEncrypted, false, 2);
    printPckData(NULL, *pckDataAdd, false, 0);

    // printf2("pointer: %p\n",pckDataEncrypted);
    // printf2("BOOOOM: %d\n",pckDataLen);
    // pckDataToNetworkOrder(pckDataAdd);
    // pckDataToNetworkOrder(pckDataEncrypted);
    // print2("Final composed node message add data",*pckDataAdd,150,0);
    return 0;
}

// int composeBeaconPacket(unsigned char *beaconData, uint8_t beaconDataLen, unsigned char **pckDataEncrypted, unsigned char **pckDataAdd){
//     nodeCmdInfo currentNodeCmdInfo;
//     memcpy(currentNodeCmdInfo.devId,localNodeSettings.devId,DEVIDLEN);
//     currentNodeCmdInfo.devType = localNodeSettings.devType;
//     currentNodeCmdInfo.opcode = 0;
//     currentNodeCmdInfo.argsLen = beaconDataLen;
//     currentNodeCmdInfo.args = beaconData;
//     composeNodeMessage(&currentNodeCmdInfo, pckDataEncrypted, pckDataAdd);

//     return 0;
// }

int composeBeaconPacketCmdInfo(nodeCmdInfo *cmdInfo, unsigned char *beaconData, uint8_t beaconDataLen)
{
    memcpy(cmdInfo->devId, localNodeSettings.devId, DEVIDLEN);
    cmdInfo->devType = localNodeSettings.devType;
    cmdInfo->opcode = 0;
    cmdInfo->argsLen = beaconDataLen;
    cmdInfo->args = malloc(beaconDataLen);
    memcpy(cmdInfo->args, beaconData, beaconDataLen);
    return 0;
}

//Specific beacon commands for each devtype

#if targetDevtype == 2 //Smart lock

int composeSmartLockBeaconCmdInfo(nodeCmdInfo *cmdInfo, uint8_t lockState, uint8_t doorState, uint8_t *password, uint8_t passwordLen){
    // memcpy(cmdInfo->devId, localNodeSettings.devId, DEVIDLEN);
    // cmdInfo->devType = localNodeSettings.devType;
    // cmdInfo->opcode = 0;
    // cmdInfo->argsLen = 2;

    // unsigned char buffer[2];
    // memcpy(buffer,&lockState,1);
    // memcpy(buffer+1, &doorState,1);
    
    // cmdInfo->args = malloc(cmdInfo->argsLen);
    // memcpy(cmdInfo->args,buffer,2);

    unsigned char buffer[3+passwordLen];
    memcpy(buffer,&lockState,1);
    memcpy(buffer+1, &doorState,1);
    memcpy(buffer+2, &passwordLen,1);
    memcpy(buffer+3, password,passwordLen);
    composeBeaconPacketCmdInfo(cmdInfo, buffer,3+passwordLen);
    return 0;
}

#endif

#if targetDevtype == 5

int composeNoiseMonitorBeaconCmdInfo(nodeCmdInfo *cmdInfo, int noiseLevel){
    // memcpy(cmdInfo->devId, localNodeSettings.devId, DEVIDLEN);
    // cmdInfo->devType = localNodeSettings.devType;
    // cmdInfo->opcode = 0;
    // cmdInfo->argsLen = 2;

    // unsigned char buffer[2];
    // memcpy(buffer,&lockState,1);
    // memcpy(buffer+1, &doorState,1);
    
    // cmdInfo->args = malloc(cmdInfo->argsLen);
    // memcpy(cmdInfo->args,buffer,2);

    unsigned char buffer[1];
    // uint8_t dbLevel = (uint8_t)floor(noiseLevel);
    // int dbLevelInt = (int)dbLevelFlood;
    int dbLevelInt = noiseLevel;
    printf2("NOISE LEVEL ORIGINAL: %d\n",dbLevelInt);
    uint8_t dbLevelFinal = (uint8_t)dbLevelInt;
    printf2("NOISE LEVEL ORIGINAL2: %d\n",dbLevelFinal);
    // printf2("dbLevelInt: %d\n",dbLevelInt);
    // uint8_t dbLevel = (uint8_t)dbLevelInt;
    // printf2("dbLevelFinal:%d \n",dbLevel);
    // printf2("Sending noise level: %d\n",dbLevel);
    memcpy(buffer,&dbLevelFinal,1);
    composeBeaconPacketCmdInfo(cmdInfo, buffer,1);
    return 0;
}
#endif


int freeNodeCmdInfo(nodeCmdInfo *cmdInfo)
{
    free(cmdInfo->args);
    return 0;
}

int loadInNodeSettings()
{
    // Open the file

#if targetPlatform == 2
    // Use NVS for data storage
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("nvs", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf2("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        printf2("Opened NVS storage successfully. Proceeding to load in settings\n");
    }


    //CHECK IF THIS IS A NEW DEVICE AND INITIALIZE FACTORY SETTINGS HERE
    
    initializeDeviceFactorySettings(&my_handle); //TODO: TEMPORARY ALWAYS LOAD FACTORY SETTINGS

    

    uint8_t isInitialized = 100;
    err = nvs_get_u8(my_handle, "isInitialized", &isInitialized);
    switch (err)
    {
    case ESP_OK:
        printf2("Device Initialized Value = %d\n", isInitialized);
        if(isInitialized==1){
            //Device is initialized
            printf2("Device is initialized\n");
        }else{
            //Device not initialized. Generate factory settings
            initializeDeviceFactorySettings(&my_handle);
        }
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        printf2("Device not initialized. Initializing default setting and generating DEVID\n");
            initializeDeviceFactorySettings(&my_handle);
        break;
    default:
        printf2("Error reading device initialization value\n", esp_err_to_name(err));
    }

    unsigned char devidMem[DEVIDLEN];
    size_t devidLength = DEVIDLEN;
    err = nvs_get_blob(my_handle, "devid", devidMem, &(devidLength));
    switch (err)
    {
    case ESP_OK:
        printf2("Acquired DEVID from memory: ");
        for(int i = 0; i <DEVIDLEN; i++){
            printf("%c",devidMem[i]);
        }
        printf("\n");
        memcpy(localNodeSettings.devId,devidMem,DEVIDLEN);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        printf2("Devid not initialized in memory. THIS SHOULD ONLY HAPPEN WHEN DEVICE IS FIRST TURNED ON\n");
        printf2("Newly created DEVID: ");
        for(int i = 0; i <DEVIDLEN; i++){
            printf("%c",localNodeSettings.devId[i]);
        }
        printf("\n");
        break;
    default:
        printf2("Error (%s) reading devid!\n", esp_err_to_name(err));
    }

    // Shared settings for all node devices
    size_t secretKeyLength = 16;
    err = nvs_get_blob(my_handle, "encryptionKey", pointerToKey, &(secretKeyLength));
    switch (err)
    {
    case ESP_OK:
        printf2("Acquired encryption key from non-volatile memory\n");
        printf2("Encryption Key = %s\n", pointerToKey);
        initGCM(&encryptionContext,pointerToKey,KEYLEN*8);
        hasEncryptionContextBeenEstablished = 1;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        printf2("Encryption Key not initialized in non-volatile memory.\n");
        break;
    default:
        printf2("Error (%s) reading the encryption key!\n", esp_err_to_name(err));
    }

#if targetDevtype == 2

    // GET PASSCODE LENGTH
    printf2("Getting passcode length from memory\n");
    uint8_t passcodeLenMem;
    uint8_t wasPasscodeLenInMemory = 0;
    err = nvs_get_u8(my_handle, "passcodeLen", &passcodeLenMem);
    switch (err)
    {
    case ESP_OK:
        printf2("Acquired passcode from non-volatile memory\n");
        printf2("Current passcode length = %d\n", passcodeLenMem);
        wasPasscodeLenInMemory = 1;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        printf2("Passcode length not initialized in non-volatile memory. Using factory value length: 4\n");

        break;
    default:
        printf2("Error (%s) reading the passcode length!\n", esp_err_to_name(err));
    }

    if (wasPasscodeLenInMemory == 1)
    {
        printf2("Getting passcode from memory\n");
        uint8_t passcodeValTemp[MAX_PASSWORD_LEN];
        err = nvs_get_blob(my_handle, "passcode",passcodeValTemp,&(passcodeLenMem));
        switch (err)
        {
        case ESP_OK:
            printf2("Acquired passcode from non-volatile memory.\n");
            // for(int i = 0; i<passcodeLenMem; i++){
            //     printf("%d",atoi(passcodeValTemp[i]));
            // }
            // printf("\n");
            setPasscode(passcodeValTemp,passcodeLenMem,0);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf2("Passcode not initialized in non-volatile memory. Using factory value: 000000\n");
            uint8_t factoryPasscode[4] = {1,1,1,1};
            setPasscode(factoryPasscode,4,0);
            break;
        default:
            printf2("Error (%s) reading the passcode!\n", esp_err_to_name(err));
        }
    }
    else
    {
        //Since no length was in memory, use factory passcode value
        printf2("Setting factory passcode: 1111\n");
        uint8_t factoryPasscode[4] = {1,1,1,1};
        setPasscode(factoryPasscode,4,0);
    }
    nvs_close(my_handle);
    return 1;
#endif

#if targetDevtype==3
    return 1;
#endif

#if targetDevtype==4
    return 1;
#endif

#if targetDevtype==5
    return 1;
#endif

#elif targetPlatform == 1
    printf2("Loading node settings config file\n");
    FILE *settingsFile;
    char temp[60];
    int currentCharacters;

    if ((settingsFile = fopen(nodeSettingsFileName, "r")) == NULL)
    {
        printf2("Failed opening node settings file\n");
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

        printf2("Loading in node settings...\n");

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
        // Analyze the obtained setting name and values and dealloc
        if (strcmp(settingName, "devId") == 0)
        {
            memcpy(localNodeSettings.devId, settingValue, DEVIDLEN);
        }
        else if (strcmp(settingName, "devType") == 0)
        {
            localNodeSettings.devType = atoi(settingValue);
        }
        printf2("CALLING FREE\n");
        free(settingName);
        free(settingValue);
    }
    fclose(settingsFile);
    printf2("Finished reading node settings\n");
    return 1;
// #elif targetPlatform == 2
// printf2("Opening node settings file using esp32's filesystem\n");

// return 1;
#endif
}

int initializeConnInfo(connInfo_t *connInfo, int socket)
{
    connInfo->connectionType = 0;
    connInfo->localNonce = 0;
    connInfo->remoteNonce = 0;
    connInfo->rcvSessionIncrements = 0;
    connInfo->sendSessionIncrements = 0;
    connInfo->sessionId = 0;
    connInfo->socket = socket;

    return 0;
}

int initializeCommandQueue(arrayList *commandQueue)
{
    pthread_mutex_lock(&commandQueueAccessMux);
    initList(&nodeCommandsQueue, sizeOfSerializedCmdInfo);
    pthread_mutex_unlock(&commandQueueAccessMux);
    return 0;
}

// Gets the first item in queue and removes it from the queue
int popCommandQueue(arrayList *commandQueue, nodeCmdInfo *cmdInfo)
{
    pthread_mutex_lock(&commandQueueAccessMux);
    unsigned char *serializedCmdInfo;
    serializedCmdInfo = getFromList(commandQueue, 0);
    deserializeCmdInfo(cmdInfo, serializedCmdInfo);
    removeFromList(commandQueue, 0);
    pthread_mutex_unlock(&commandQueueAccessMux);
    return 0;
}

int addToCommandQueue(arrayList *commandQueue, nodeCmdInfo *cmdInfo)
{
    pthread_mutex_lock(&commandQueueAccessMux);
    unsigned char serializedCmdInfo[sizeOfSerializedCmdInfo];
    serializeCmdInfo(serializedCmdInfo, cmdInfo);
    addToList(commandQueue, serializedCmdInfo);
    pthread_mutex_unlock(&commandQueueAccessMux);
    return 0;
}

int getCommandQueueLength(arrayList *commandQueue)
{
    pthread_mutex_lock(&commandQueueAccessMux);
    int len = commandQueue->length;
    pthread_mutex_unlock(&commandQueueAccessMux);
    return len;
}

void sleep_ms(int millis)
{
// printf("Sleeping for %i ms\n",millis);
#if targetPlatform == 1
    struct timespec ts;
    ts.tv_sec = millis / 1000;
    ts.tv_nsec = (millis % 1000) * 1000000;
    nanosleep(&ts, NULL);
#elif targetPlatform == 2
    vTaskDelay(millis / portTICK_RATE_MS);
#endif
}

// The output structure should have size as variable: sizeOfSerializedCmdInfo.
void serializeCmdInfo(unsigned char *output, nodeCmdInfo *input)
{
    memcpy(output, input->devId, DEVIDLEN);
    *((uint32_t *)(output + DEVIDLEN)) = input->devType;
    *((uint32_t *)(output + DEVIDLEN + 4)) = input->opcode;
    *((uint32_t *)(output + DEVIDLEN + 4 + 4)) = input->argsLen;
    memcpy(output + DEVIDLEN + 4 + 4 + 4, &(input->args), sizeof(unsigned char *));
}

void deserializeCmdInfo(nodeCmdInfo *output, unsigned char *input)
{
    memcpy(output->devId, input, DEVIDLEN);
    output->devType = *((uint32_t *)(input + DEVIDLEN));
    output->opcode = *((uint32_t *)(input + DEVIDLEN + 4));
    output->argsLen = *((uint32_t *)(input + DEVIDLEN + 4 + 4));
    memcpy(&(output->args), input + DEVIDLEN + 4 + 4 + 4, sizeof(unsigned char *));
}

// Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...)
{
    int result;
    va_list args;
    va_start(args, formattedInput);
    printf(ANSI_COLOR_BLUE "NodeClient: " ANSI_COLOR_WHITE);
    result = vprintf(formattedInput, args);
    va_end(args);
    return result;
}

int saveEncryptionKeyToMemory(unsigned char *key){
    printf2("Saving encryption key to memory\n");
    nvs_handle_t handle;
    esp_err_t err = nvs_open("nvs", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return -1;
    } else {
        printf("Opened NVS memory. Saving the new encryption key\n");
    }
    nvs_set_blob(handle,"encryptionKey",key,16);
    nvs_close(handle);
    return 1;

}

// ESP 32 specific functions
#if targetPlatform == 2

void initializeDeviceFactorySettings(nvs_handle *nvsHandle){

    //Generate random devid string and store in memory
    printf2("INITIALIZING FACTORY SETTINGS FOR DEVICE\n");
    
    //Delete any settings that were already present
    nvs_erase_all(*nvsHandle);

    char devidStr[DEVIDLEN];
    rand_string(devidStr,DEVIDLEN);

    printf2("Generated DEVID: ");
    for(int i = 0; i<DEVIDLEN;i++){
        printf("%c",devidStr[i]);
    }
    printf("\n");
    nvs_set_blob(*nvsHandle,"devid",devidStr,DEVIDLEN);

    memcpy(localNodeSettings.devId,devidStr,DEVIDLEN);


    //Finished initializing
    esp_err_t err = nvs_set_u8(*nvsHandle,"isInitialized",1);

    nvs_commit(*nvsHandle);

    printf2("Error: %s\n",esp_err_to_name(err));

}

// Inits NVS and SPIFFS
void initStorage()
{
    printf2("Initializing SPIFFS filesystem");
    // INIT NVS (key:value pairs storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // INIT SPIFFS (storage filesystem)
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            printf2("Failed to mount or format filesystem\n");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            printf2("Failed to find SPIFFS partition\n");
        }
        else
        {
            printf2("Failed to initialize SPIFFS (%s)\n", esp_err_to_name(ret));
        }
    }

    // Test SPIFF
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        printf2("Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        printf2("SPIFF partition size: total: %d, used: %d", total, used);
    }
}

void initWifiStationMode(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifiEventHandler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifiEventHandler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = networkSSID,
            .password = networkPassword,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 networkSSID, networkPassword);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 networkSSID, networkPassword);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

static void wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < wifiMaximumReconnectAttempts)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
}

#endif