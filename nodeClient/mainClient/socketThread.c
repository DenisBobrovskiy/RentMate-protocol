#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <netdb.h>
#include <memory.h>
#include <pthread.h>
#include "client.h"
#include <sys/random.h>
#include <unistd.h>
#include <sys/socket.h>
#include "compileFlag.h"
#include "socketThread.h"

#if targetPlatform == 1
#include "pckData.h"
#include "generalSettings.h"
#include "ecdh.h"
#elif targetPlatform == 2
#include "pckData.h"
#include "ecdh.h"
#include "generalSettings.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#endif

#if targetDevtype == 2 //Smart lock
#include "./smartLock/smartLockCore.h"
#endif

#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_WHITE "\x1B[37m"
#define connectionTimeoutSeconds 3 // In seconds


int packetRecievingTimeoutMs = 500; // In milliseconds, how long to wait for new packets from server after a broadcast packet
int reconnectAttempts = 0; //This is the value of reconnection attempts done to server after a connection failed, if exceeds MAX_RECONNECT_ATTEMPS - reset 
//controller device's IP and wait for a UDP broadcast with new IP

connInfo_t connInfo; //Stores connection information. Reset it when no connection is established

unsigned char sendProcessingBuffer[MAXMSGLEN];
unsigned char recvProcessingBuffer[MAXMSGLEN];
int yes = 1;

fd_set fdsetMain;
int socketMain;
int hasConnected = 0;

struct timeval connectionTimeout;
struct timeval recvPacketTimeout;
char mainControllerIp[INET_ADDRSTRLEN] = "NONE";
uint16_t port = 3333;

struct sockaddr_in newConnAddr;
socklen_t sockLen;

int initializeSocket()
{
    connectionTimeout.tv_sec = connectionTimeoutSeconds;
    connectionTimeout.tv_usec = 0;
    recvPacketTimeout.tv_sec = packetRecievingTimeoutMs / 1000;
    recvPacketTimeout.tv_usec = (packetRecievingTimeoutMs % 1000) * 1000;

    // Use this to initialize decryption key from node settings file (assuming the key has been previously established)
    /* initGCM(&encryptionContext, pointerToKey, KEYLEN * 8); */
    /* hasEncryptionContextBeenEstablished = 1; */

    // INITIALIZING SOCKET AND CONNECTING
    printf2("Initializing the socket\n");
    socketMain = socket(AF_INET, SOCK_STREAM, 0);
    initializeConnInfo(&connInfo, socketMain);
    // setsockopt(socketMain, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    newConnAddr.sin_addr.s_addr = inet_addr(mainControllerIp);
    newConnAddr.sin_family = AF_INET;
    newConnAddr.sin_port = htons(port);
    sockLen = sizeof(newConnAddr);
    if (fcntl(socketMain, F_SETFL, O_NONBLOCK) == -1)
    {
        // Error setting socket non-blocking
        perror("Error setting socket non-blocking: ");
        return -1;
    }

    // Set socket timeout for recieving data
    setsockopt(socketMain, SOL_SOCKET, SO_RCVTIMEO, (const char *)&recvPacketTimeout, sizeof recvPacketTimeout);

    return 0;
}

void *socketThread(void *args)
{

    bool connectionClosedByServer;
    // establish connection at wake up intervals (adjustable)
    while (1)
    {
        // First wait to find ip of the server from a UDP broadcast message
        if (strcmp(mainControllerIp, "NONE") == 0)
        {
            // Main controller not found yet
            int broadcastRecvSocket;
            struct sockaddr_in broadcastAddr, broadcastSourceAddr;
            int yes = 1;
            char broadcastMessageBuffer[100];
            socklen_t broadcastAddrLen = sizeof(broadcastAddr);
            memset(broadcastMessageBuffer, 0, sizeof(broadcastMessageBuffer));

            broadcastRecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (broadcastRecvSocket < 0)
            {
                printf2("Error creating broadcast recv socket\n");
                perror("Error:");
                reboot(1);
            }
            printf2("Broadcast recv socket created successfully\n");
            setsockopt(broadcastRecvSocket, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
            broadcastAddr.sin_family = AF_INET;
            broadcastAddr.sin_port = htons(6666);
            broadcastAddr.sin_addr.s_addr = INADDR_ANY;

            // Bind socket
            if (bind(broadcastRecvSocket, (struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr)) < 0)
            {
                printf2("Couldnt bind broadcast socket\n");
                perror("Error:");
                reboot(1);
            }

            // Wait for broadcast message
            while (1)
            {
                printf2("Waiting for broadcast message\n");
                if (recvfrom(broadcastRecvSocket, broadcastMessageBuffer, sizeof(broadcastMessageBuffer), 0, (struct sockaddr *)(&broadcastSourceAddr), &broadcastAddrLen) < 0)
                {
                    printf2("Error recieving broadcast message\n");
                    continue;
                }
                printf2("Broadcast message recieved: %s\n", broadcastMessageBuffer);

                //Free the UDP socket
                close(broadcastRecvSocket);


                if (memcmp(broadcastMessageBuffer, broadcastMessageExpected, 16) == 0)
                {
                    printf2("Recieved broadcast from main controller!\n");
                    // Now get the remote ip and save it to use to connect later
                    struct in_addr ipAddr = broadcastSourceAddr.sin_addr;
                    char ipAddrStr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &ipAddr, ipAddrStr, INET_ADDRSTRLEN);
                    memcpy(mainControllerIp, ipAddrStr, INET_ADDRSTRLEN);
#if localPlatform == 1
                    // The server is on same device as this one, so just set ip to 127.0.0.1
                    printf2("LOCAL PLATFORM\n");
                    char localIp[INET_ADDRSTRLEN] = "127.0.0.1";
                    memcpy(mainControllerIp, localIp, INET_ADDRSTRLEN);
#endif
                    printf2("Main controller ip: %s\n", mainControllerIp);
                    break;
                }
                memset(broadcastMessageBuffer, 0, sizeof(broadcastMessageBuffer));
            }
        }

        connectionClosedByServer = false;
        initializeSocket();
        if (connect(socketMain, (struct sockaddr *)&newConnAddr, sockLen) == -1)
        {
            // printf2("Connection failed\n");
        }
        FD_ZERO(&fdsetMain);
        FD_SET(socketMain, &fdsetMain);
        hasConnected = 0;
        printf2("Starting select\n");
        if (select(socketMain + 1, NULL, &fdsetMain, NULL, &connectionTimeout))
        {
            int so_error;
            socklen_t len = sizeof(so_error);
            printf2("Running select()\n");
            getsockopt(socketMain, SOL_SOCKET, SO_ERROR, &so_error, &len);
            perror("Error: ");
            printf2("so error: %d\n", so_error);
            if (so_error == 0)
            {
                // Socket connected
                printf2("Connected successfully (in select())\n");
                hasConnected = 1;
            }
        }
        // After accepted connection, set socket to blocking again (Because it doesnt need to be non-blocking to have a timeout, unlike for connection call, where we need to use non-blocking socket with select to have a connection attempt timeout)
        //  int opts = opts = opts & (~O_NONBLOCK);
        int opts = 0;
        opts = opts & (~O_NONBLOCK);
        if (fcntl(socketMain, F_SETFL, opts) == -1)
        {
            // Error resetting socket back to blocking mode
            perror("Eror resetting socket back to blocking mode: ");
        }

        if (hasConnected == 1)
        {
            // Figure out if the encryption key has been established
            if (hasEncryptionContextBeenEstablished == 0)
            {
                // Key not established
                printf2("Encryption key hasnt been exchanged yet. Starting diffie helmann key exchange with the server\n");
                // Send the message, unencrypted, where extra data will contain the key for diffie helmann
                printf2("ECC_PRV_KEY_SIZE: %d\n", ECC_PRV_KEY_SIZE);

                //Generate random data for private and public DH keys generation

                #if targetPlatform==1
                getrandom(privateDHKeyBuffer, ECC_PRV_KEY_SIZE, 0);
                #elif targetPlatform==2
                esp_fill_random(privateDHKeyBuffer,ECC_PRV_KEY_SIZE); //WIFI MODULE NEEDS TO BE ENABLED FOR THIS TO BE TRUE RANDOM NUMBER
                #endif


                ecdh_generate_keys(publicDHKeyLocalBuffer, privateDHKeyBuffer);
                unsigned char *extraDataPckData;
                unsigned char *addPckData;
                initPckData(&addPckData);
                uint8_t connType = 1;
                uint8_t targetDevtypeVar = targetDevtype;
                appendToPckData(&addPckData, (unsigned char *)&(localNodeSettings.devId), DEVIDLEN);
                appendToPckData(&addPckData, &targetDevtypeVar, 1);
                appendToPckData(&addPckData, &(connType), 1); //TODO: Change this after implementing more connection types
                initPckData(&extraDataPckData);
                appendToPckData(&extraDataPckData, publicDHKeyLocalBuffer, ECC_PUB_KEY_SIZE);
                encryptAndSendAll(socketMain, 0, NULL, NULL, NULL, addPckData, extraDataPckData, 0, sendProcessingBuffer);

                // Wait to recv the reply containing the servers key to generate shared secret
                recvAll(&recvHolders, NULL, socketMain, recvProcessingBuffer, processMsg);

                //Check if recvAll returned -1
            }
            else if (hasEncryptionContextBeenEstablished == 1)
            {

                // Establish sessionID
                if (connInfo.localNonce == 0)
                {
                    // Generate a localNonce
                    unsigned char *encryptedPckData = NULL;
                    unsigned char *addPckData;
                    // For ADD pck data we need DEVID so we can identify which device we are talking to
                    initPckData(&addPckData);
                    appendToPckData(&addPckData, (unsigned char *)&(localNodeSettings.devId), DEVIDLEN);
                    unsigned char *extraPckData = NULL;

                    if (encryptAndSendAll(socketMain, 0, &connInfo, &encryptionContext, encryptedPckData, addPckData, extraPckData, 0, sendProcessingBuffer) == -1)
                    {
                        printf2("Failed sending the localNonce...\n");
                    }

                    // Wait for remote nonce
                    while (connInfo.sessionId == 0)
                    {
                        printf2("Waiting\n");
                        printf2("Waiting to establish sessionID\n");
                        int status = recvAll(&recvHolders, &connInfo, socketMain, recvProcessingBuffer, processMsg);
                        if (status == 0)
                        {
                            // Connection was closed
                            /* printf2("Connection closed by server\n"); */
                            /* close(socketMain); */
                            /* connectionClosedByServer = true; */
                            // NOT CLOSED IT JUST GETS 0 BYTES AT THE END OF THE LOOP
                            break;
                        }
                        else if (status == -1)
                        {
                            // Error occured
                            printf2("Error recieving the message\n");
                            printf2("Error:%s\n", strerror(errno));
                            break;
                        }
                    }
                }

                // Check if sessionID (and therefore connection) is established and then send out any pending commands and recieve any pending commands
                if (connInfo.sessionId != 0)
                {
                    // First, always send a corresponding beacon packet for corresponding devtype

#if targetDevtype == 2 // Smart lock beacon
                    nodeCmdInfo beaconCmdInfo;
                    printf2("Sending lock beacon packet\n");
                    composeSmartLockBeaconCmdInfo(&beaconCmdInfo, currentLockState, currentDoorState, currentPasscode, currentPasscodeLen);
                    addToCommandQueue(&nodeCommandsQueue, &beaconCmdInfo);
#endif

                    nodeCmdInfo currentCmdInfo;
                    unsigned char *pckDataEncrypted;
                    unsigned char *pckDataAdd;
                    unsigned char outBuf[MAXMSGLEN];
                    uint32_t tempGSettings = 0; // For now have empty settings (until i actually add settings)
                    int numOfCommands = getCommandQueueLength(&nodeCommandsQueue);
                    printf2("Num of commands to send out: %d\n", numOfCommands);
                    for (int i = 0; i < numOfCommands; i++)
                    {
                        popCommandQueue(&nodeCommandsQueue, &currentCmdInfo);
                        print2("DEVID TO BE SENT", currentCmdInfo.devId, 16, 0);
                        composeNodeMessage(&currentCmdInfo, &pckDataEncrypted, &pckDataAdd);
                        encryptAndSendAll(socketMain, 0, &connInfo, &encryptionContext, pckDataEncrypted, pckDataAdd, NULL, tempGSettings, outBuf);
                    }

                    // Recieve any pending commands
                    printf2("start of recv...\n");
                    int ret = recvAll(&recvHolders, &connInfo, socketMain, recvProcessingBuffer, processMsg);
                    printf2("end of recv...\n");
                    if (ret == 1)
                    {
                        // Connection closed
                        initializeConnInfo(&connInfo, 0);
                        printf2("Connection closed from server\n");
                        close(socketMain);
                        connectionClosedByServer = true;
                    }
                }
                else
                {
                    // Connection not established, do nothing
                }

                // Close connection after recieving and sending all commands
                if (!connectionClosedByServer)
                {
                    initializeConnInfo(&connInfo, 0); // reset command info
                    printf2("Finished sending and recieving commands. Closing connection to server\n");
                    close(socketMain);
                }
            }
        }
        else
        {
            // If connection failed
            reconnectAttempts++;
            printf2("Connection failed. Connection attempts failed: %d. Going to sleep\n",reconnectAttempts);

            //If connection fails too many times
            //Could be due to IP change (Controller device got restarted), hence reset IP and wait for UDP broadcast

            if(reconnectAttempts>MAX_RECONNECT_ATTEMPTS){
                strcpy(mainControllerIp,"NONE");
                reconnectAttempts = 0;
            }


            initializeConnInfo(&connInfo, 0); //Reset connection info
            close(socketMain); //close socket //TODO: maybe combine close and resetting conninfo into one function? close1()?
        }
        sleep_ms(wakeUpIntervalMs); //How long to sleep TODO: actually implement sleep mode
    }
    pthread_exit(0);
}

static int printf2(char *formattedInput, ...)
{
    int result;
    va_list args;
    va_start(args, formattedInput);
    printf(ANSI_COLOR_YELLOW "SocketThread: " ANSI_COLOR_WHITE);
    result = vprintf(formattedInput, args);
    va_end(args);
    return result;
}
