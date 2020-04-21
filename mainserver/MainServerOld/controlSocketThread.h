#ifndef CONTROLSOCKTHREAD_H
#define CONTROLSOCKTHREAD_H
#include <stdint.h>

void *controlSockThread(void *args);

//PROCESSING CODE 
void getMsgLen(uint16_t *msgLen, char *msgLenSpecifier,unsigned char **currentMsgStartPtr, unsigned char *msgBufPtr, int rs);
int recvAllCmdThread(int socket, unsigned char *msgBuf, uint32_t msgBufLen, unsigned char *processBuf, int (*processessingBufCallback)());
void handleMsg(uint16_t *msgLen,
                uint16_t *msgLenRecieved, 
                char *msgLenSpecifier,
                unsigned char **currentMsgStartPtr, 
                unsigned char *msgBufPtr, 
                int msgBufLen, 
                int rs,
                unsigned char *processMsgBuf,
                int (*processessingBufCallback)());

int processMsgCallback(int msgLen);
typedef struct commandRequestCommandControll{
	uint32_t opCode;
	unsigned char isReservedOpCode;
	unsigned char args; //Malloc based on argsLen
	uint32_t argsLen;
	unsigned char *addPtr; //Malloc based on addLem
	unsigned char addLen;
	unsigned char gSettings;
}commandRequestCommandControll_t;
#endif