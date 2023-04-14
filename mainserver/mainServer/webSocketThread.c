
#include "webSocketThread.h"
#include "time.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "../../misc/json-c/build/json.h"
#include "server.h"

#define WEBSOCKET_BUFFER_LENGTH 15000

// COLORS
#define ANSI_COLOR_PURPLE "\e[40;38;5;171m"
#define ANSI_COLOR_RESET "\x1b[0m"

// Websocket command queue. Add commands here to be sent to the remote server
arrayList websocketCommandQueue;
pthread_mutex_t websocketCommandQueueMux;

enum protocols
{
    MAIN_PROTOCOL = 0,
    PROTOCOL_COUNT
};

struct lws_protocols protocols[] =
    {
        {
            "webSocketClient",
            webSocketCallback,
            0,
            WEBSOCKET_BUFFER_LENGTH,
        },
        {NULL, NULL, 0, 0} /* terminator */
};

struct lws *web_socket = NULL;
int connectionAuthenticated = 0;        // 0 if not; 1 if yes; 2 if in progress
struct json_object *readDataBuf = NULL; // This is a buffer to use for reading JSON values when recieving a message from server

// Sequence number data
int currentSeqNum = 0;

// Remote action request from server
remoteActionRequest actionRequest = GetDeviceData;

void *websocketThread(void *args)
{
    printf2("Started websocket thread\n");

    // Initialize websocket command queue
    initList(&websocketCommandQueue, sizeof(webscoketCommand_t));

    // Always keep a websocket connection open
    // Establish websocket context
    // Connection data
    char inputUrl[100] = "ws://localhost:11000";
    // Context data
    struct lws_client_connect_info clientContextInfo; // Client creation info
    struct lws_context_creation_info contextInfo;
    struct lws_context *context;
    // URL data
    const char *urlProtocol; // the protocol of the URL, and a temporary pointer to the path
    char *urlPath;           // The final path string

    // Initialize context
    memset(&contextInfo, 0, sizeof(contextInfo));
    memset(&clientContextInfo, 0, sizeof(clientContextInfo));

    // Parse the connection URL
    printf2("URL INPUT PATH: %s\n", inputUrl);
    if (lws_parse_uri(inputUrl, &urlProtocol, &clientContextInfo.address, &clientContextInfo.port, &urlPath))
    {
        printf2("Couldn't parse URL\n");
    }
    // printf2("URL INPUT PATH: %s\n",inputUrl);
    // printf2("URL TEMP PATH: %s\n",urlTempPath);
    printf2("urlProtocol: %s\n", urlProtocol);
    printf2("urlAddress: %s\n", clientContextInfo.address);
    printf2("urlPort: %d\n", clientContextInfo.port);
    printf2("urlPath: %s\n", urlPath);

    // Set client context info
    clientContextInfo.path = urlPath;

    // Set context info
    contextInfo.port = CONTEXT_PORT_NO_LISTEN; // Client shouldnt listen to any connections
    contextInfo.protocols = protocols;         // Use our protocol list
    contextInfo.gid = -1;
    contextInfo.uid = -1;
    // Create context
    context = lws_create_context(&contextInfo);
    if (context == NULL)
    {
        printf2("Eror during context creation\n");
        return 1;
    }

    time_t oldTimeSec = 0; // Old time in seconds
    while (1)
    {
        // printf2("Waiting\n");
        // Get current time. We will use it so we attempt a connection every second.
        struct timeval currentTime;
        gettimeofday(&currentTime, NULL);
        // printf2("time:%d\n", currentTime.tv_sec);
        /* Connect if we are not connected to the server. */
        // if( !web_socket && tv.tv_sec != old )
        if (web_socket == NULL && (currentTime.tv_sec != oldTimeSec))
        {
            printf2("Attempting to connect websocket\n");
            clientContextInfo.context = context;
            clientContextInfo.ssl_connection = 0; // No SSL for now
            clientContextInfo.host = clientContextInfo.address;
            clientContextInfo.origin = clientContextInfo.address;
            clientContextInfo.ietf_version_or_minus_one = -1;
            clientContextInfo.protocol = protocols[MAIN_PROTOCOL].name;
            // clientContextInfo.protocol = webProtocol.name;
            clientContextInfo.pwsi = &web_socket;
            // ccinfo.address = "localhost";
            // ccinfo.origin = "origin";
            // ccinfo.port = 11000;
            // ccinfo.path = "/";
            // ccinfo.host = lws_canonical_hostname( context );
            // ccinfo.protocol = protocols[MAIN_PROTOCOL].name;
            // web_socket = lws_client_connect_via_info(&ccinfo);

            web_socket = lws_client_connect_via_info(&clientContextInfo);
        }

        // Send something only if we need to authenticate or we got messages in command queue TODO: add command queue if stuff
        if (connectionAuthenticated == 0 && web_socket != NULL)
        {
            lws_callback_on_writable(web_socket);
        }
        // old = tv.tv_sec;
        // }
        oldTimeSec = currentTime.tv_sec;
        lws_service(context, /* timeout_ms = */ 250);
    }

    // Connection loop broken, shouldnt happen
    printf2("Connection loop broken\n");
    lws_context_destroy(context);

    return 0;
}

int webSocketCallback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    printf2("Websocket callback called\n");

    switch (reason)
    {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
    {
        // Connection established
        printf2("Connection established\n");
        // lws_callback_on_writable(wsi);
        break;
    }

    case LWS_CALLBACK_CLIENT_RECEIVE:
    {
        // Handle messages from server here
        printf2("Message recieved\n");
        printf2("Data received: %s\n", in);
        char *buf = malloc(len);
        memcpy(buf, in, len);
        struct json_object *msgData = json_tokener_parse(buf);
        json_bool exists = false;

        // Message handling

        // If this response returns status 0 for whatever reason, make sure the socket is deauthenticated
        exists = json_object_object_get_ex(msgData, "state", &readDataBuf);
        if (exists == false)
        {
            // Every message needs to send status, so if no status is received ignore this message
            printf2("No message state recieved. Ignoring this message\n");
            break;
        }
        int statusValue = json_object_get_int64(readDataBuf);
        if (statusValue == 0)
        {
            connectionAuthenticated = 0;
        }
        else if (statusValue == 1)
        {
            connectionAuthenticated = 1;
        }

        // If connection not authenticated ignore this message
        if (connectionAuthenticated == 0)
        {
            printf2("Connection not authenticated. Ignoring this message\n");
            break;
        }

        // Act upon an action request
        exists = false;
        exists = json_object_object_get_ex(msgData, "action", &readDataBuf);
        if (exists == false)
        {
            // No action data. Hence ignore this message
            printf2("No action value recieved from server. Ignoring this message\n");
            break;
        }
        char *actionString = json_object_get_string(readDataBuf);
        printf2("Action request recieved: %s\n", actionString);
        if (strcmp(actionString, "deviceCommand") == 0)
        {
            // Create the command request for node device
            commandRequestCommand_t command;

            // Get the DEVID
            exists = json_object_object_get_ex(msgData, "devid", &readDataBuf);
            if (exists == false)
            {
                // No DEVID present.
            }
            unsigned char *devid = json_object_get_string(readDataBuf);

            // Get the opcode
            exists = json_object_object_get_ex(msgData, "opcode", &readDataBuf);
            uint32_t opcode = json_object_get_int(readDataBuf);
            command.opCode = opcode;

            // Get reservedOpCode status
            exists = json_object_object_get_ex(msgData, "isReservedOpcode", &readDataBuf);
            uint32_t isReservedOpcode = json_object_get_int(readDataBuf);
            command.isReservedOpCode = isReservedOpcode;

            // //Get args len data
            // exists = json_object_object_get_ex(msgData,"argsLen",&readDataBuf);
            // uint32_t argsLen = json_object_get_int(readDataBuf);
            // command.argsLen = argsLen;

            // //Get add args len data
            // exists = json_object_object_get_ex(msgData,"addLen",&readDataBuf);
            // uint32_t addLen = json_object_get_int(readDataBuf);
            // command.addLen = addLen;

            // Get the args data
            exists = json_object_object_get_ex(msgData, "args", &readDataBuf);
            uint32_t argsLen_builtin = json_object_get_string_len(readDataBuf); // Get the argsLen provided automatically by json library
            unsigned char *args = json_object_get_string(readDataBuf);
            command.argsLen = argsLen_builtin;
            command.argsPtr = malloc(argsLen_builtin);
            memcpy(command.argsPtr, args, argsLen_builtin);

            // Get the add args data
            exists = json_object_object_get_ex(msgData, "addData", &readDataBuf);
            uint32_t addLen_builtin = json_object_get_string_len(readDataBuf); // Get the argsLen provided automatically by json library
            unsigned char *addData = json_object_get_string(readDataBuf);
            command.addLen = addLen_builtin;
            command.addPtr = malloc(addLen_builtin);
            memcpy(command.addPtr, addData, addLen_builtin);

            // Get gSettings bitmap
            exists = json_object_object_get_ex(msgData, "gSettings", &readDataBuf);
            uint32_t gSettings = json_object_get_int(readDataBuf);
            command.gSettings = gSettings;

            // Add command request to command queue
            printf2("Adding message to command queue\n");
            addToCommandQueue(devid, &command);

            // Create the command request for node device
            printf2("devid: %s. opcode: %d. argsLen: %d. addLen: %d. gSettings:%d. args: %s. add: %s\n", devid, opcode, argsLen_builtin, addLen_builtin, gSettings, args, addData);
        }
        else if (strcmp(actionString, "getDeviceInfo") == 0)
        {
            printf2("Processing get device data request from remote server\n");
            actionRequest = GetDeviceData;
            lws_callback_on_writable(wsi);
        }

        // Check if this a response to authenticate request

        free(buf);
        break;
    }

    case LWS_CALLBACK_CLIENT_WRITEABLE:
    {
        // Send message to server
        printf2("Sending message to server\n");
        char *message;
        int n = 0;
        if (connectionAuthenticated == 0)
        {
            struct json_object *authDetails;
            authDetails = json_object_new_object();
            json_object_object_add(authDetails, "username", json_object_new_string(localSettings.username));
            json_object_object_add(authDetails, "propertyId", json_object_new_string_len(localSettings.propertyId, 24));
            json_object_object_add(authDetails, "propertyKey", json_object_new_string(localSettings.propertyKey));
            json_object_object_add(authDetails, "action", json_object_new_string("authenticate"));
            json_object_object_add(authDetails, "seqNum", json_object_new_int64(currentSeqNum));
            message = json_object_to_json_string_length(authDetails, 0, &n);
            // Connection not authenticated, send credentails to authenticate
            connectionAuthenticated = 2;
            printf2("%s\n", message);
            // connectionAuthenticated = 1; //Make it authenticated if the server confirms correct credentials

            // Get message length;
            printf2("Message length: %d\n", n);
            if (n > 0)
            {
                unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WEBSOCKET_BUFFER_LENGTH + LWS_SEND_BUFFER_POST_PADDING];
                unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
                memcpy(p, message, n);
                currentSeqNum++;
                lws_write(wsi, p, n, LWS_WRITE_TEXT);
                break;
            }
        }
        else
        {
            // Authenticated. Send the queued command messages

            // Check if the server has requested anything
            if (actionRequest == None)
            {
                // Do nothing
            }
            else if (actionRequest == GetDeviceData)
            {
                printf2("GetDeviceData request recieved\n");
                // Send back the devid data for all known devices
                struct json_object *newMessageDetails;
                newMessageDetails = json_object_new_object();

                //Mark the message as GetDeviceDataResponse action
                json_object_object_add(newMessageDetails,"action",json_object_new_string("GetDeviceInfoResponse"));

                //Add seq number. This doesnt matter in this case.
                json_object_object_add(newMessageDetails,"seqNum",json_object_new_uint64(1));

                // Json array for device data
                struct json_object *deviceDataArray;
                deviceDataArray = json_object_new_array();

                for (int i = 0; i < devInfos.length; i++)
                {
                    printf2("Device data found. Adding to command\n");
                    devInfo *deviceData = getFromList(&devInfos, i);
                    json_object *deviceDataEntry = json_object_new_object();
                    json_object_object_add(deviceDataEntry, "devId", json_object_new_string_len(deviceData->devId, DEVIDLEN));
                    json_object_object_add(deviceDataEntry, "devtype", json_object_new_uint64(deviceData->devType));
                    json_object_object_add(deviceDataEntry,"connectionType", json_object_new_uint64(deviceData->deviceConnectionType));
                    
                    //Calculate time since last connection
                    time_t currentTime;
                    time(&currentTime);
                    double timeDiff = difftime(currentTime,deviceData->lastConnectionTime);
                    int timeDiffInt = (uint64_t)timeDiff;
                    printf2("time difference = %d\n",timeDiffInt);

                    //Add how long ago last connection to node device was in seconds
                    json_object_object_add(deviceDataEntry,"lastConnectionTime",json_object_new_uint64(timeDiffInt));
                    

                    //Smart lock data
                    if(deviceData->devType==2){
                        json_object_object_add(deviceDataEntry,"lockStatus",json_object_new_uint64(deviceData->lockState));
                        json_object_object_add(deviceDataEntry,"doorStatus",json_object_new_uint64(deviceData->doorState));

                        //
                        printf2("input decimal val:%d\n",deviceData->password[0]);
                        printf2("first char: %c\n",(char)((deviceData->password[0])+"0"[0]));

                        char passcodeStr[12];
                        //Convert uint8_t password array to string 
                        for(int i = 0; i<deviceData->passwordLen+1; i++){
                            char val = (char)((deviceData->password[0])+"0"[0]);
                            passcodeStr[i] = val;
                            // if(i== deviceData->passwordLen){
                            //     passcodeStr[i] = 0;
                            // }
                        }
                        json_object_object_add(deviceDataEntry,"password",json_object_new_string_len(passcodeStr,deviceData->passwordLen));
                    }

                    if(deviceData->devType==5){
                        json_object_object_add(deviceDataEntry,"noiseLevel",json_object_new_uint64(deviceData->noiseLevel));
                    }

                    json_object_array_add(deviceDataArray, deviceDataEntry);
                }

                if(devInfos.length>0){
                    json_object_object_add(newMessageDetails,"status",json_object_new_string("devicesFound"));
                    json_object_object_add(newMessageDetails,"deviceData",deviceDataArray);
                    message = json_object_to_json_string_length(newMessageDetails, 0, &n);
                }else{
                    message = "{state:'noDevicesConnected'}";
                    n = 28;
                }
                if (n > 0)
                {
                    printf2("%s\n", message);
                }

                unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WEBSOCKET_BUFFER_LENGTH + LWS_SEND_BUFFER_POST_PADDING];
                unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
                memcpy(p, message, n);
                currentSeqNum++;
                lws_write(wsi, p, n, LWS_WRITE_TEXT);
                break;
            }

            // Check if device info was requested

            // Iterate through commands queue for specific device commands
            // for (int i = 0; i < websocketCommandQueue.length; i++)
            // {
            //     webscoketCommand_t *command;
            //     command = getFromList(&websocketCommandQueue,i);
            //     struct json_object *newMessageDetails;
            //     newMessageDetails = json_object_new_object();

            //     json_object_object_add(newMessageDetails, "username", json_object_new_string(localSettings.username));
            //     json_object_object_add(newMessageDetails, "propertyId", json_object_new_string_len(localSettings.propertyId, 24));
            //     json_object_object_add(newMessageDetails, "propertyKey", json_object_new_string(localSettings.propertyKey));
            //     json_object_object_add(newMessageDetails, "devId", json_object_new_string_len(command->devtype,DEVIDLEN));
            //     json_object_object_add(newMessageDetails, "devType", json_object_new_int64(command->devtype));
            //     json_object_object_add(newMessageDetails, "opcode", json_object_new_int64(command->opcode)); // What action the device wants to communicate to server
            //     json_object_object_add(newMessageDetails, "args", json_object_new_string_len(command->args,command->argsLen));  // What action the device wants to communicate to server
            //     json_object_object_add(newMessageDetails, "action", json_object_new_string("deviceCommand"));
            //     json_object_object_add(newMessageDetails, "seqNum", json_object_new_int64(currentSeqNum));
            //     message = json_object_to_json_string_length(newMessageDetails, 0, &n);

            //     //Send the message
            //     // Get message length;
            //     printf2("Message length: %d\n", n);
            //     if (n > 0)
            //     {
            //         unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WEBSOCKET_BUFFER_LENGTH + LWS_SEND_BUFFER_POST_PADDING];
            //         unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
            //         memcpy(p, message, n);
            //         currentSeqNum++;
            //         lws_write(wsi, p, n, LWS_WRITE_TEXT);
            //         break;
            //     }
            // }
        }
    }

    case (LWS_CALLBACK_CLOSED):
    {
        // Connection closed
        printf2("Websocket connection closed\n");
        web_socket = NULL;
        connectionAuthenticated = 0;
        break;
    }

    case (LWS_CALLBACK_CLIENT_CLOSED):
    {
        // Connection closed
        printf2("Websocket connection closed\n");
        web_socket = NULL;
        connectionAuthenticated = 0;
        break;
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    {
        // Connection error while connecting to server
        printf2("Connection error to server\n");
        web_socket = NULL;
        connectionAuthenticated = 0;
        break;
    }

    case LWS_CALLBACK_EVENT_WAIT_CANCELLED:
    {
        printf2("Event wait callback cancelled\n");
        // web_socket = NULL;
        // connectionAuthenticated = 0;
        break;
    }

    default:
    {
        printf2("Default websocket callback called. Reason: %d\n", reason);
        break;
    }
    }
    return 0;
}

void addWebsocketCommand(unsigned char *devid, uint8_t devtype, uint64_t opcode, unsigned char *args, uint64_t argsLen)
{
    printf2("Adding to websocket command queue\n");
    pthread_mutex_lock(&websocketCommandQueueMux);

    webscoketCommand_t newCommand;
    newCommand.devtype = devtype;
    newCommand.opcode = opcode;
    newCommand.argsLen = argsLen;
    memcpy(newCommand.devid, devid, DEVIDLEN);
    memcpy(newCommand.args, args, argsLen);

    addToList(&websocketCommandQueue, &newCommand);

    pthread_mutex_unlock(&websocketCommandQueueMux);
}

// Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...)
{
    int result;
    va_list args;
    va_start(args, formattedInput);
    printf(ANSI_COLOR_PURPLE "webSocketThread: " ANSI_COLOR_RESET);
    result = vprintf(formattedInput, args);
    va_end(args);
    return result;
}