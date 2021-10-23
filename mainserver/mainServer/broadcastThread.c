
#include "broadcastThread.h"
#include "pckData/generalSettings.h"
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include "server.h"
#include "pckData/pckData.h"
#include <stddef.h>
#include <stdlib.h>

#define ANSI_COLOR_LIGHTCYAN "\e[40;38;5;51m"
#define ANSI_COLOR_RESET "\x1b[0m"

int broadcastSocket;

void *broadcastThread(void *args){
    printf2("Starting broadcast thread\n");
    struct sockaddr_in serverAddr, clientAddr;
    char broadcastMessage[16] = "BroadcastV100000";

    //Initialize broadcast socket
    if((broadcastSocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP))<0){
        //Error
        printf2("Socket creation failed\n");
        perror("Error: ");
        exit(1);
    }
    //Give socket broadcast permissions
    int yes = 1;
    setsockopt(broadcastSocket,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));
    memset(&serverAddr,0,sizeof(serverAddr));
    memset(&clientAddr,0,sizeof(clientAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(BroadcastPort);
    serverAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    printf2("Started broadcast socket successfully\n");

    //Sending broadcast message in a loop
    while(1){
        printf2("Sending broadcast message\n");
        if(sendto(broadcastSocket,broadcastMessage,strlen(broadcastMessage),0,(struct sockaddr*)&serverAddr,sizeof(struct sockaddr_in))<0){
            //Error during send
            printf2("Error sending broadcast packet\n");
            perror("Error:");
            exit(1);
        }
        sleep_ms(3000);

    }

    pthread_exit((void*)0);
}

void sleep_ms(int millis)
{
//printf("Sleeping for %i ms\n",millis);
    struct timespec ts;
    ts.tv_sec = millis / 1000;
    ts.tv_nsec = (millis % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

static int printf2(char *formattedInput, ...){
    int result;
    va_list args;
    va_start(args,formattedInput);
    printf(ANSI_COLOR_LIGHTCYAN "broadcastThread: " ANSI_COLOR_RESET);
    result = vprintf(formattedInput,args);
    va_end(args);
    return result;
}
