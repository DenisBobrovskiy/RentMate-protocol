#ifndef MSGSENDER_HEADER_H
#define MSGSENDER_HEADER_H

void print3(unsigned char *data, int len);
/*int composeMsg(mbedtls_gcm_context *ctx,
                unsigned char *msgBuf, 
                unsigned char *devId, 
                unsigned char *iv, 
                unsigned char *add, 
                unsigned char addLen,
                uint32_t nonce, 
                unsigned char generalSettings,
                uint32_t opCode,
                unsigned char *commandsArgs,
                uint32_t commandArgsLen);
*/
void initGeneralSettings(unsigned char *gSettings, unsigned char isTerminator, unsigned char isOpCodeReserved);
void getMsgLen(uint16_t *msgLen, char *msgLenSpecifier,unsigned char **currentMsgStartPtr, unsigned char *msgBufPtr, int rs);
/*int recvAll(int socket, unsigned char *msgBuf, uint32_t msgBufLen, unsigned char *processBuf, int (*processessingBufCallback)());*/
void handleMsg(uint16_t *msgLen,
                uint16_t *msgLenRecieved, 
                char *msgLenSpecifier,
                unsigned char **currentMsgStartPtr, 
                unsigned char *msgBufPtr, 
                int msgBufLen, 
                int rs,
                unsigned char *processMsgBuf,
                int (*processessingBufCallback)());
int processProcessBuffer(int msgLen);
void sleep_ms(int millis);

#endif