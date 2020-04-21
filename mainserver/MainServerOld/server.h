#ifndef MAIN_SERVER_HEADER_H
#define MAIN_SERVER_HEADER_H

#include <stdint.h>
#include "generalSettings.h"
#include "ArrayList/arrayList.h"
#include "../../protocol/pckData/pckData.h"

//GLOBALS
extern pthread_mutex_t recvHolderMutex;
extern pthread_mutex_t commandQueueAccessMux;
extern arrayList recvHolders;
extern int counter;
extern arrayList commandsQueue;

//CIRCULAR BUFFER
	typedef struct _cirBuf
	{
		void *data;
		int lastIndex;
		int sizeOfBuf;
	}
	circularBuffer;


int initBuf(circularBuffer *buf, int size);
int addToBuf(circularBuffer *buf, void *data, int length);
void printCirBuf(circularBuffer *buf);

/*
//SOCKET HELPERS
typedef struct _recvHolder
{
	int socket;
	unsigned char *recvQueue;
    int size;
	int sizeUsed;
    unsigned char *firstMsgPtr;
    int16_t firstMsgSize;
	int16_t firstMsgSizeUsed;
	int timeSinceUsed;
}
recvHolder;
*/

//int initRecvHolder(recvHolder *holder, int maxSize, int socket);
//int resetRecvHolder(recvHolder *holder,int socket);
int initCommandsRequestQueue();
//int recvAll(recvHolder *holder, int socket);
//int recvAll2(arrayList *recvHoldersList, int socket);
//int removeRecvHolder(arrayList *recvHolders, int index);
//int handleMsgLen(recvHolder *recvHolderToUse, int rs);
//int handleMsgData(recvHolder *recvHolderToUse, int rs);
//void circulateBuffer(recvHolder *recvHolderToUse, int rs);


typedef struct _commandRequest{
	unsigned char devId[DEVIDLEN];
	arrayList commandRequestsCommands;
}commandRequest_t;

typedef struct _commandRequestCommand{
	uint32_t opCode;
	unsigned char isReservedOpCode;
	unsigned char *argsPtr; //Malloc based on argsLen
	uint32_t argsLen;
	unsigned char *addPtr; //Malloc based on addLem
	unsigned char addLen;
	unsigned char gSettings;
}commandRequestCommand_t;

int freeCommandRequest(int index);
int addConnInfo(int s, uint32_t recvNonce, uint32_t sendNonce);
int removeConnInfo(int s);
connInfo_t *findConnInfo(int s, arrayList *connInfos);
int findDevIdInCmdQueue(unsigned char *devId);
//int closeSocket(int s);

//MESSAGE HANDLING
int processMsg();
int generateCommandsMsg(unsigned char *newMsgBuf, commandRequestCommand_t *newCmdReq, unsigned char *devId, connInfo_t *connInfo, unsigned char gSettings);
int processSendQueue(int socket);

int addToCmdQueue(arrayList *cmdReqQueue,
				unsigned char *devId,
				uint32_t opCode, 
				unsigned char gSettings, 
				unsigned char *argsPtr, 
				uint32_t argsLen, 
				unsigned char *addPtr, 
				unsigned char addLen);

//int sendall(int s, unsigned char *buf, int len);


//SETTINGS
int modifySetting(unsigned char *settingName, int settingLen, uint32_t newOption);

#endif
