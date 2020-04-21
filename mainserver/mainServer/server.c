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

//CUSTOM
#include "mbedtls/include/mbedtls/gcm.h"
#include "ArrayList/arrayList.h"
#include "AES-GCM/aes-gcm.h"
#include "pckData/pckData.h"
#include "pckData/generalSettings.h"
#include "server.h"
#include "epollRecievingThread.h"

#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_RESET "\x1b[0m"

//MULTITHREADING
pthread_t epollRecievingThreadID; //Thread that listens for node and user commands


arrayList connectionsInfo;  //Store info for every connection to server
arrayList recvHolders;  //Necesarry for storing recieved data sent in multiple packets. Used in a more advanced implementation of recv() function.
arrayList devInfos; //Stores information for every device known to the server
arrayList commandsQueue;    //Stores the commands recieved from a client(used not node). Queued up for execution
globalSettingsStruct globalSettings;    //global settings. Loaded and saved in a config plaintext file


int main(void){
    //INITIALIZATION
    printf2("Initializing main server\n");
    //modifySetting("passExchangeMethod", 18, 0);
    initBasicServerData(&connectionsInfo, &recvHolders, &devInfos, &commandsQueue, &globalSettings, 0);
    pthread_create(&epollRecievingThreadID,NULL,epollRecievingThread,NULL);
    pthread_join(epollRecievingThreadID,NULL);
}

//Processes a message, it can be any kind of message(node msg or user msg)
int processMsg(connInfo_t *connInfo, unsigned char *msg)
{
    printf2("Processing new message\n");

    //If caught beacon packet. Send it to the list of unverified devices, waiting for the user to accept/reject it. No persistent storage, just during runtime. Allow the option of instantly accepting all devices for debugging purposes.


}

//Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...){
    int result;
    va_list args;
    va_start(args,formattedInput);
    printf(ANSI_COLOR_GREEN "mainServer: " ANSI_COLOR_RESET);
    result = vprintf(formattedInput,args);
    va_end(args);
    return result;
}
