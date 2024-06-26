#ifndef PCKDATA_H
#define PCKDATA_H


#include "generalSettings.h"

#if targetPlatform==2
#include "aes-gcm.h"
#include "arrayList.h"
#include "mbedtls/gcm.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#elif targetPlatform==1
#include "aes-gcm.h"
#include "arrayList.h"
#include "gcm.h"
#endif

#include <stdint.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <stdbool.h>

//DEFINES
#define pckDataIncrements 255   //If pckData(a binary data containing structure) gets filled and u need to add more to it, it will increment by this number

//PROTOCOL SPECIFIC MACROS
// #define DEVIDLEN 12   //length of DEVID, a unique permament id of a device
// #define IVLEN 12    //Length of the IV (encryption stuff), 12 is okay
// #define TAGLEN 16   //Length of tag, tag is used to verify integrity of the message is AES-GCM protocol
// #define NONCELEN 4  //Length of nonce, a random number used to establish a unique session before starting to communicate, prevents replay attacks
// #define KEYLEN 16   //Length of enryption key in AES-GCM
// #define MAXDEVMSGLEN 255    //Max length of a message coming from a device
#define protocolSpecificAddLen 4   //MODIFY THIS IF YOU ADD ANY NEW DATA TO PROTOCOL SPECIFIC ADD
#define protocolSpecificExtraDataLen 0  //MODIFY THIS IF YOU ADD ANY NEW DATA TO PROTOCOL SPECIFIC EXTRA DATA (UNLIKE ADD IT CAN BE MODIFIED BY 3rd PARTIES)
#define protocolSpecificEncryptedDataLen 8 // 4 bytes for nonce, 4 bytes for general settings field


//STRUCTS---------------------------------------------------------------------------------------

//Contains information about a connection to another socket, gets created whenever a connection is established
typedef struct _connInfo{
    int socket;
    char connectionType;  //0 - Connection between server and node. 1 - Connection between user and node in LAN. 2 - Connection between user and server from internet
    //SessionId - a unique random number is established and then incremented by 1 for every message exchanged.
    uint32_t remoteNonce;   //random number from the remote socket to establish the sessionID. it is 0 if not yet set
    uint32_t localNonce;    //random number from the local socket to establish the sessionID. it is 0 if not yet set
    uint32_t sessionId;     //SessionId, unique number for every session, prevents replay attacks. If it is 0 it means it hasnt been set
    uint32_t sendSessionIncrements;     //Before sending a message increment by 1, allows sessionId to be different for every message during session.
    uint32_t rcvSessionIncrements;      //Before reading message increment by 1 (because every new message sent will have its sessionId incremented by 1)

}connInfo_t;

//A structure used in recvAll() function, helps to capture a segmented message
typedef struct _recvHolder
{
	int socket;
	unsigned char *recvQueue;
    int size;
	int sizeUsed;
    unsigned char *firstMsgPtr;
    uint32_t firstMsgSize;
	uint32_t firstMsgSizeUsed;
	int timeSinceUsed;
}
recvHolder;

//Global settings for any device (server or node)
typedef struct _globalSettingsStruct{
	int passExchangeMethod;
}globalSettingsStruct;

//Identifies a node, its password and type. The server stores a list of those for every node known to it
typedef struct _devInfo{
	unsigned char devId[DEVIDLEN]; //Unique device identifier (like a MAC address)
	unsigned char key[KEYLEN];  //AES-GCM encryption key for this device, generate and exchange on first connection
	uint32_t devType;   //Type of device. If is 0, then it isnt set.
    
    uint8_t deviceConnectionType; 
    // 0 - undefined;
    //1 - interval (for power low devices that only send beacons and recieve at certain intervals)
    //2 - connected fully (this is a fully connected device, most likely battery powered to constantly maintain wifi connection)

    uint8_t deviceConnectionStatus;
    //0 - disconnected (for fully connected device this is when socket connection is interrupted)
    //1 - connected (for interval connected device this is when a device hasnt reconnected for too long TODO: implement)


    time_t lastConnectionTime;
    
    //Fields for smart lock
    uint8_t lockState;
    uint8_t doorState;
    uint8_t passwordLen;
    uint8_t password[12];

    //Fields for smart socket
    uint8_t socketStatus;

    //Fields for noise monitor
    uint8_t noiseLevel;
    uint8_t noiseThresholdReached;

    //Fields for presence detector
    uint8_t presenceDetected;
}devInfo;

//Structure storing a list of commands that needs to be executed for every DEVID
typedef struct _commandRequest{
	unsigned char devId[DEVIDLEN]; //devID of target device
	arrayList commandRequestsCommands; //list of commands to be executed (commandRequestCommand struct) 
}commandRequest_t;


//A singular command for a node to execute
typedef struct _commandRequestCommand{
	uint32_t opCode;
	unsigned char isReservedOpCode;
	unsigned char *argsPtr; //Malloc based on argsLen
	uint32_t argsLen;
	unsigned char *addPtr; //Malloc based on addLen
	unsigned char addLen;
	uint32_t gSettings;
}commandRequestCommand_t;

//FUNCTIONS----------------------------------------------------------------------------------
//Initializes the pckData structure, always call before use
int initPckData(unsigned char **pckData);
//
//Appends a new element to pckData
int appendToPckData(unsigned char **pckData, unsigned char *newData, uint32_t newDataLen);

//Generates an encrypted message (Read README.md for encrypted packet structurej)
int encryptPckData(mbedtls_gcm_context *ctx,
                    unsigned char *pckData,
                    unsigned char *addDataPck,
                    unsigned char *extraDataPck,
                    uint32_t pckGSettings,
                    uint32_t nonce,
                    unsigned char *outputBuf
                    );

//Decrypts a previously encrypted mesasge with key stored in *ctx
int decryptPckData(mbedtls_gcm_context *ctx, unsigned char *encryptedMsg, unsigned char *outputBuf);

//Used for quickly printing out arrays of a specific datatype 
//datatype = 0: prints bytes in integer form
//datatype = 1: prints bytes in character form
//datatype = 2: prints 32 bit numbers
void print2(char *labelMsg, unsigned char *data, int length, int datatype);

typedef int (*processingCallback)(connInfo_t *connInfo, unsigned char *processingBuf);
void resetRecvHolder(recvHolder *holder,int socket);
int initRecvHolder(recvHolder *holder, int maxSize, int socket);
int removeRecvHolder(arrayList *recvHolders, int index);
void circulateBuffer(recvHolder *recvHolderToUse, int rs);
int handleMsgData(connInfo_t *connInfo, recvHolder *recvHolderToUse, int rs, unsigned char *processMsgBuffer, processingCallback processingMsgCallback);
void handleMsgLen(recvHolder *recvHolderToUse, int rs);

int recvAll(arrayList *recvHoldersList, connInfo_t *connInfo, int socket, unsigned char *processBuf, processingCallback processingMsgCallback);
int accept1(int socket, struct sockaddr *newAddr, socklen_t *newLen, arrayList *connInfos, char connType);
int close1(int socket, arrayList *connInfos);
int connect1(int socket, const struct sockaddr* addr, int sockaddrLen, arrayList *connInfos);
int closeSocket(int socket);
int processMsg();
int sendall(int s, unsigned char *buf, int len);
int readDecryptedData(unsigned char *decryptedData, arrayList *decryptedDataPointers);
/* uint32_t getElementFromPckData(arrayList *pointersToData, unsigned char **ptrToElement, int index); */
uint32_t getElementFromPckData(unsigned char *pckData, unsigned char *output, int index);
uint32_t getElementLengthFromPckData(unsigned char *pckData, int index);
int pckDataToNetworkOrder(unsigned char *pckData);
int pckDataToHostOrder(unsigned char *pckData);
unsigned char* getPointerToUserAddData(unsigned char *msgPtr);

uint32_t getNonceFromDecryptedData(unsigned char *decryptedDataPtr);
uint32_t getPckGSettingsFromDecryptedData(unsigned char *decryptedDataPtr);


int initBasicClientData(arrayList *recvHolders,
        globalSettingsStruct *globalSettings,
        uint32_t devType);

int initBasicServerData(arrayList *connInfos,
        arrayList *recvHolders,
        arrayList *devInfos,
        arrayList *commandsQueue,
        globalSettingsStruct *globalSettings,
        uint32_t devType
        );

int deinitBasicClientData(arrayList *recvHolders);
int deinitBasicServerData(arrayList *connInfos, arrayList *recvHolders, arrayList *devInfos, arrayList *commandsQueue);

// connInfo_t* findConnInfo(int socket, arrayList *connInfosList);

int encryptAndSendAll(int socket,
                    uint32_t sendFlags,
                    connInfo_t *connInfo,
                    mbedtls_gcm_context *ctx,
                    unsigned char *pckData,
                    unsigned char *addDataPck,
                    unsigned char *extraDataPck,
                    uint32_t pckGSettings,
                    unsigned char *outBuf
                    );


int initializeSettingsData(globalSettingsStruct *globals);

int printfFormatted(char *prefix, char *formattedInput, ...);
static int printf2(char *formattedInput, ...);

connInfo_t *findConnInfo(arrayList *connInfos, int socket);
int removeConnInfo(arrayList *connInfos, int socket);

/* int readDataEntry(arrayList *dataEntries, unsigned char **dataPtr, int index); */


void printMessage(char *labelMsg, unsigned char *data, int datatype, bool isEncrypted, bool isSerialized);
void printPckData(char *label, unsigned char *pckData, int isSerialized, int colorscheme);
void reboot(int exitCode);

#endif
