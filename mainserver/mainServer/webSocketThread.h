#ifndef WEBSOCKETTHREAD_HEADER_H
#define WEBSOCKETTHREAD_HEADER_H

#include "libwebsockets.h"
#include "server.h"

//Type of action requested from remote server
typedef enum _remoteActionRequest{
    None,
    GetDeviceData
}remoteActionRequest;

typedef struct _websocketCommand_t{
    unsigned char devid[DEVIDLEN];
    uint64_t devtype;
    uint64_t opcode;
    unsigned char *args;
    uint64_t argsLen;
}webscoketCommand_t;

static int printf2(char *formattedInput, ...);
void *websocketThread(void *args);
int webSocketCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);
void addWebsocketCommand(unsigned char *devid, uint8_t devtype, uint64_t opcode, unsigned char *args, uint64_t argsLen);

#endif