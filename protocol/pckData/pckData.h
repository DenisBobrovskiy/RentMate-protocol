#ifndef PCKDATA_H
#define PCKDATA_H

#include "AES-GCM/aes-gcm.h"
#include "ArrayList/arrayList.h"
#include "mbedtls/include/mbedtls/gcm.h"
#include <stdint.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include "generalSettings.h"

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
}devInfo;

//Structure created by the user to tell a node to execute a list of commands, gets sent from a frontend client to the server(LAN or NAT) and then server quees them up and sends to nodes
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
	unsigned char *addPtr; //Malloc based on addLem
	unsigned char addLen;
	unsigned char gSettings;
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
void print2(char labelMsg[255], unsigned char *data, int length, int datatype);

typedef int (*processingCallback)(connInfo_t *connInfo, unsigned char *processingBuf);
int resetRecvHolder(recvHolder *holder,int socket);
int initRecvHolder(recvHolder *holder, int maxSize, int socket);
int removeRecvHolder(arrayList *recvHolders, int index);
void circulateBuffer(recvHolder *recvHolderToUse, int rs);
int handleMsgData(connInfo_t *connInfo, recvHolder *recvHolderToUse, int rs, unsigned char *processMsgBuffer, processingCallback processingMsgCallback);
int handleMsgLen(recvHolder *recvHolderToUse, int rs);

int recvAll(arrayList *recvHoldersList, connInfo_t *connInfo, int socket, unsigned char *processBuf, processingCallback processingMsgCallback);
int accept1(int socket, struct sockaddr *newAddr, socklen_t *newLen, arrayList *connInfos, char connType);
int closeSocket(int socket);
int processMsg();
int sendall(int s, unsigned char *buf, int len);
int readDecryptedData(unsigned char *decryptedData, arrayList *decryptedDataPointers);
uint32_t getElementFromPckData(arrayList *pointersToData, unsigned char **ptrToElement, int index);
int readExtraData(unsigned char *msgPtr, uint32_t addLen, arrayList *extraDataPointers);
int readAddData(unsigned char *msgPtr, arrayList *addDataPointers);
int pckDataToNetworkOrder(unsigned char *pckData);
int pckDataToHostOrder(unsigned char *pckData);
unsigned char* getPointerToUserAddData(unsigned char *msgPtr);

uint32_t getNonceFromDecryptedData(unsigned char *decryptedDataPtr);
uint32_t getPckGSettingsFromDecryptedData(unsigned char *decryptedDataPtr);

static int initPckDataInfoBasic(arrayList *connInfos, arrayList *recvHolders, uint32_t devType);
static int deinitPckDataInfoBasic(arrayList *connInfos, arrayList *recvHolders);

int initBasicClientData(arrayList *connInfos,
        arrayList *recvHolders,
        globalSettingsStruct *globalSettings,
        uint32_t devType);

int initBasicServerData(arrayList *connInfos,
        arrayList *recvHolders,
        arrayList *devInfos,
        arrayList *commandsQueue,
        globalSettingsStruct *globalSettings,
        uint32_t devType
        );

int deinitBasicClientData(arrayList *connInfos, arrayList *recvHolders);
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

// int sendPartOfSessionId(arrayList *connInfos, int socket);
// int recievePartOfSessionId(arrayList *connInfos, arrayList *recvHolders, int socket, unsigned char *processingBuffer);
// static int internalRecievePartOfSessionId(connInfo_t *connInfo, unsigned char *processingBuf);

int initializeSettingsData(globalSettingsStruct *globals);

int printfFormatted(char *prefix, char *formattedInput, ...);
static int printf2(char *formattedInput, ...);

connInfo_t *findConnInfo(arrayList *connInfos, int socket);

#endif
