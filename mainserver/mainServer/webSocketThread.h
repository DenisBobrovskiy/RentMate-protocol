#ifndef WEBSOCKETTHREAD_HEADER_H
#define WEBSOCKETTHREAD_HEADER_H

#include "libwebsockets.h"

static int printf2(char *formattedInput, ...);
void *websocketThread(void *args);
int webSocketCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

#endif