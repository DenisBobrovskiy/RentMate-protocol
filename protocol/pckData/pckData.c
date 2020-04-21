//Custom packet handling. Uses aes-gcm for encryption and custom pckData structure for simply generating packets
/*
//BASIC USE GUIDE:
->Use pckData structures and their handler functions for storing and sending over data
->Use connect1() instead of connect(). It allows to keep connState working
->Use close1() instead of close(). It allows to keep connState working
->Use accept1() instead of accept(). It allows to keep connState working
->Use recvAll() for recieving data
->Always establish sessionId before sending messages (see below)

//DATA PACKETS
->Every packet contains pckData, addPckData, extPckData. These are pckData structures(see above) for data. pckData contains data to be encrypted.
addPckData contains aadData (see AES-GCM protocol), data that is not encrypted but can be verified so it cant be modified by 3rd party but can be
seen by 3rd party. extPckData is data that can be seen by 3rd party and can be modified by 3rd party use ONLY for irrelevant data.
->Apart from data every packet will contain an IV(Encryption unique number), tag - used to verify the validity of message in AES-GCM protocol and
fields for message lengths, settings and more.

//SESSION INITIATION
->Before sending messages a sessionId has to be established between the server and the client:
(Desription) the connection initiator sends a random number to its target. It stores it and sends its own random number to the connection initiator.
Numbers are encrypted with the device's corresponding key. Then they add these two numbers together to get a unique sessionID. Once established
every time you send a message or recieve one increment the connInfo->sessionIdIncrements by 1 to have unique sessionId every single message
There are two separated increment fields for a recieving and a sending, thats so in case both devices send a message to each other at same time
the sessionId's dont get messed up.
(Steps)
1) Send


*/
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/random.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include "pckData.h"
#include "commands.h"
#include "AES-GCM/aes-gcm.h"
#include "ArrayList/arrayList.h"


unsigned char settingsFileName[30] = "settings.conf";

//inits all the defaults on a client side application
int initBasicClientData(arrayList *connInfos,
        arrayList *recvHolders,
        globalSettingsStruct *globalSettings,
        uint32_t devType){

    initPckDataInfoBasic(connInfos, recvHolders, devType);
    initializeSettingsData(globalSettings);
    return 0;
}

//inits all the defaults on a server side application
//recvHolders - a list for recvHolders (needed for function recvAll()), just pass a non-inititalized, empty arrayList.
int initBasicServerData(arrayList *connInfos,
        arrayList *recvHolders,
        arrayList *devInfos,
        arrayList *commandsQueue,
        globalSettingsStruct *globalSettings,
        uint32_t devType
        ){

    initPckDataInfoBasic(connInfos,recvHolders, devType);

    //Allocate devIDs list to keep track of devices
    if(initList(devInfos,sizeof(devInfo))!=0){
        printf("Failed to init DevId list\n");
        exit(1);
    }
	if(initList(commandsQueue,sizeof(commandRequest_t))!=0){
		printf("Failed to init command list\n");
		exit(1);
    }

    initializeSettingsData(globalSettings);
    return 0;
}


//deinits all the defaults on a server side application
int deinitBasicServerData(arrayList *connInfos, arrayList *recvHolders, arrayList *devInfos, arrayList *commandsQueue){
    deinitPckDataInfoBasic(connInfos, recvHolders);
    freeArrayList(devInfos);
    freeArrayList(commandsQueue);
    return 0;

}

int deinitBasicClientData(arrayList *connInfos, arrayList *recvHolders){
    deinitPckDataInfoBasic(connInfos, recvHolders);
    return 0;
}

//Inits basic stuff (both client & server, it gets called on every single device's boot)
//connInfos - a list for keeping connInfos. Needed on both a client and a server machine. Pass a non-initialzed, empty arrayList
static int initPckDataInfoBasic(arrayList *connInfos, arrayList *recvHolders, uint32_t devType){
    if(initList(connInfos,sizeof(connInfo_t))==-1){
        printf("Failed initializing connInfos list\n");
        return -1;
    }
    //init recvHolers list for recvAll() function
    if(initList(recvHolders,sizeof(recvHolder))!=0){
        printf("Failed to init recvHoldersList on serverInit");
        return -1;
    }
    //Init the opCodes
    initOpCodes(devType);

    return 0;
}

//Deinits all defaults on client and server, get called on every shutdown
static int deinitPckDataInfoBasic(arrayList *connInfos, arrayList *recvHolders){
    freeArrayList(connInfos);
    freeArrayList(recvHolders);
    return 0;
}

//Custom connect(). Use instead of normal connect. Allows to keep track of conenctionInfos(sessionIDs) and can be expended even more.
//Returns -1 on error
int connect1(int socket, const struct sockaddr* addr, int sockaddrLen, arrayList *connInfos){
    socklen_t sockLen = sockaddrLen;
    if(connect(socket,addr,sockLen)==-1){
        printf("Connection failed\n");
        return -1;
    }
    //If connecting succeeded, add a new connInfo
    connInfo_t newConnInfo;
    newConnInfo.sessionIdState = 0;
    //newConnInfo.sessionId = 0;
    //newConnInfo.sessionId = 0;
    addToList(connInfos,&newConnInfo);
    return 0;
}

//Custom accept(). Allows to keep track of connInfos (sessionIDs). If accept() successeds a new conn info for socker gets added to arrayList connInfos
//Returns -1 on error
int accept1(int socket, struct sockaddr *newAddr, socklen_t *newLen, arrayList *connInfos){
    int newFd = accept(socket,newAddr,newLen);
    if(newFd==-1){
        printf("Accepting connection failed\n");
        return -1;
    }
    //If successfully accepted, add to connInfos
    connInfo_t newConnInfo;
    newConnInfo.socket = newFd;
    newConnInfo.sessionIdState  = 0;
    addToList(connInfos, &newConnInfo);
    return 0;
}

//Custom close(). Removes any connInfos for the socket that gets closed.
//Returns -1 on error
int close1(int socket, arrayList *connInfos){
    if(close(socket)==-1){
        printf("Failed closing connection\n");
        return -1;
    }

    //Remove connInfo for this socket
    connInfo_t *tempConnInfo = NULL;
    for(int i = 0; i<connInfos->length; i++){
        tempConnInfo = getFromList(connInfos,i);
        if(tempConnInfo->socket==socket){
            //Found it
            printf("Found connInfo to remove\n");
            removeFromList(connInfos,i);
            break;
        }
    }
    return 0;
}

//Finds the connInfo for specific socket from arrayList connInfos.
//Returns NULL if nothing found.
connInfo_t* getConnInfo(arrayList *connInfos, int socket){
    connInfo_t *tempConnInfo;
    for(int i = 0; i<connInfos->length; i++){
        tempConnInfo = getFromList(connInfos,i);
        if(tempConnInfo->socket==socket){
            printf("Found corresponding connInfo");
            return tempConnInfo;
        }
    }
    //If nothing found return NULL
    return NULL;
}

//Composes a protocol compatible message, encrypts it and then sends it to specified socket.
//socket - recieiving socket,
//sendFlags - sendFlags (argument to send() function)
//connInfos - a list of connInfo's for this socket, generated during initBasicClientData or initBasicServerData and populated during connect1(), accept1(), close1()
//mbedtls_gcm_context, AES-GCM encryption context (contains encryption key), initialialized differently depending on application
//pckData - a pointer to binary pckData datatype. Stores the body of the message and modified using handler functions such as initPckData, appendToPckData, etc. This part gets encrypted
//addPckData - a pointer to binary pckData datatype. Stores addData (AES-GCM non-encrypted but reliable data(can be verified if modified by 3rd party)).
//extraDataPck - a pointer to binary pckData datatype. Non-encrypted and non-reliable.
//pckGSettings - 32 bytes worth of message settings.
//outBuf - simply for processing needs of a function, must be maxMsgLen in length (Because message needs to be encrypted and composed so we need this buffer to temporarily store it before sending)
//Returns -1 on error
int encryptAndSendAll(int socket,
        uint32_t sendFlags,
        arrayList *connInfos,
        mbedtls_gcm_context *ctx,
        unsigned char *pckData,
        unsigned char *addDataPck,
        unsigned char *extraDataPck,
        uint32_t pckGSettings,
        unsigned char *outBuf
        )
{
    //Find corresponding connInfo to socket
    char isConnInfoFound = 0;
    connInfo_t *finalConnInfo = NULL;
    for(int i = 0; i<connInfos->length;i++){
        connInfo_t *newConnInfo = getFromList(connInfos,i);
        if(newConnInfo!=NULL){
            if(newConnInfo->socket==socket){
                //Found corresponding connInfo!
                isConnInfoFound=1;
                finalConnInfo = newConnInfo;
            }
        }
    }
    if(isConnInfoFound==0){
        return -1;
    }

    //Check if sessionId is stablsihed or not
    if(finalConnInfo->sessionIdState==1){
        //The sessionId is set!. Just increment the send counter by 1 before sending the message.
        finalConnInfo->sendSessionIncrements+=1;
        uint32_t msgLen = encryptPckData(ctx,pckData,addDataPck,extraDataPck,pckGSettings,(finalConnInfo->sessionId+finalConnInfo->sendSessionIncrements),outBuf);
        sendall(socket,outBuf,msgLen);
    }else if(finalConnInfo->sessionIdState==0){
        //The sessionId not established
        printf("SessionId not set. Aborting send request.\n");
        return -1;
    }else if(finalConnInfo->sessionIdState==2){
        //SessionId establishment is in progress
        printf("Sessionid is being established. Cant send message yet. Aborting send request.\n");
        return -2;
    }


    printf("Current SessionId: %d\n",finalConnInfo->sessionId);
    return 0;
}

//Generate pckData binary datatype. Return -1 on error. Just pass a pointer to a pointer of new pckData to this function, It manually allocates initial space
//Returns -1 on error
int initPckData(unsigned char **pckData){
    unsigned char *tempPtr;
    tempPtr = malloc(pckDataIncrements);
    if(tempPtr==NULL){
        perror("Failed mallocing pckData in initPckData");
        return -1;
    }
    *pckData = tempPtr;
    //Set totalLen to 8
    *(uint32_t*)*pckData = 8;
    *(uint32_t*)(*pckData+4) = pckDataIncrements;
    print2("called initPckData, data:",*pckData,10,0);

    return 0;
}

//Appends a new element to pckData (and updates amount of elements inside this binary datatype), reallocs more space if ran out of space
//Return: -1 on error
int appendToPckData(unsigned char **pckData, unsigned char *newData, uint32_t newDataLen){
    print2("adding data:",newData,16,1);
    uint32_t currentLen = *(uint32_t*)*pckData;
    uint32_t currentLim = *(uint32_t*)(*pckData+4);
    unsigned char *tempPtr;
    printf("---Before(Current len: %d, Current lim: %d)\n",currentLen,currentLim);
    print2("Printing data in array:",*pckData,40,0);

    while((currentLen+newDataLen)>currentLim){
        tempPtr = realloc(pckData, currentLim*2);
        if(tempPtr==NULL){
            perror("Failed reallocating pckData");
            return -1;
        }
        *pckData = tempPtr;
        currentLim = currentLim*2;
    }
    //Once enough space, assign the data
    *(uint32_t*)(*pckData+currentLen) = newDataLen;
    memcpy((*pckData+currentLen+4),newData,newDataLen);
    currentLen = currentLen+4+newDataLen;
    *(uint32_t*)*pckData = currentLen;
    *(uint32_t*)(*pckData+4) = currentLim;

    printf("---After(Current len: %d, Current lim: %d)\n",currentLen,currentLim);
    print2("Printing data in array:",*pckData,40,0);
    return 0;
}

//encrypts the pckData binary datatype (adding all protocol specific wrapping) and free() pckData.
//Returns msg len (for send()) or -1 on error
//outputBuf is just a pointer that gets malloc'ed with msgLen and then a final message ready to be sent is assigned into this buffer
int encryptPckData(mbedtls_gcm_context *ctx,
        unsigned char *pckData,
        unsigned char *addDataPck,
        unsigned char *extraDataPck,
        uint32_t pckGSettings,
        uint32_t nonce,
        unsigned char *outputBuf
        )
{
    uint32_t pckDataLen = *(uint32_t*)pckData;
    unsigned char newIv[IVLEN];
    getrandom(newIv,12,0);

    uint32_t addLen = protocolSpecificAddLen;  //Total add length
    uint32_t userAddLen = 0;  //Only user defined add length
    if(addDataPck!=NULL){
        printf("Add length adding...\n");
        userAddLen = *(uint32_t*)addDataPck;
    }
    addLen+=userAddLen;

    uint32_t extraDataLen = protocolSpecificExtraDataLen;
    uint32_t userExtraDataLen = 0;
    if(extraDataPck!=NULL){
        printf("Extra data length adding...\n");
        userExtraDataLen = *(uint32_t*)extraDataPck;
    }
    extraDataLen+=userExtraDataLen;

    uint32_t msgLen = 4 + 4 + 4 + IVLEN + TAGLEN + addLen + extraDataLen + 4 + 4 + pckDataLen;
    printf("Msg len: %d\n",msgLen);
    printf("Add len: %d\n",addLen);
    printf("Extra data len: %d\n",extraDataLen);
    outputBuf = malloc(msgLen);

    //Unencrypted data
    unsigned char *msgLenPtr = outputBuf;
    unsigned char *addLenPtr = msgLenPtr+4;
    unsigned char *extraDataLenPtr = addLenPtr+4;
    unsigned char *ivPtr = extraDataLenPtr+4;
    unsigned char *tagPtr = ivPtr+IVLEN;
    unsigned char *addPtr = tagPtr+TAGLEN;
    unsigned char *addPckDataPtr = addPtr+protocolSpecificAddLen;
    unsigned char *extraDataPtr = addPtr+addLen;  //Used for assigning protocol specific add
    unsigned char *extraDataPckPtr = extraDataPtr+protocolSpecificExtraDataLen;  //Used for assigning user defined add
    //Encrypted data
    unsigned char *noncePtr = addPtr+addLen;
    unsigned char *packetGSettingsPtr = noncePtr+4;
    unsigned char *pckDataPtr = packetGSettingsPtr+4;

    //---Assign protocol defined add
    //Version num
    *(uint32_t*)addPtr = 0;  //NOTE: Versions not implemented yet. Default to zero

    //---Assign protocol defined extra data


    //---Assign/Memcopy the data into the outputBuffer (NOTE: Assign tag after encryption is performed)
    *(uint32_t*)msgLenPtr =  htonl(msgLen);
    *(uint32_t*)addLenPtr = htonl(addLen);
    *(uint32_t*)extraDataLenPtr = htonl(extraDataLen);

    memcpy(ivPtr,newIv,IVLEN);
    *(uint32_t*)noncePtr = htonl(nonce);
    *(uint32_t*)packetGSettingsPtr = htonl(pckGSettings);
    pckDataToNetworkOrder(pckData);
    pckDataToNetworkOrder(addDataPck);
    pckDataToNetworkOrder(extraDataPck);
    memcpy(pckDataPtr,pckData,pckDataLen);
    if(userAddLen>0){
        memcpy(addPckDataPtr,addDataPck,userAddLen);
    }
    if(userExtraDataLen>0){
        memcpy(extraDataPckPtr,extraDataPck,userExtraDataLen);
    }

    print2("Msg before encryption:",outputBuf,msgLen,0);
    //Encrypt (and output tag into msg during encryption)
    if(addLen==0){
        addPtr = NULL;
    }
    if(encryptGcm(ctx,noncePtr,4+4+pckDataLen,ivPtr,IVLEN,addPtr,addLen,noncePtr,tagPtr,TAGLEN)!=0){
        printf("Failed encrypting data\n");
        return -1;
    }
    //Free
    free(pckData);

    print2("Msg after encryption:",outputBuf,msgLen,0);

    return msgLen;
}

//Converts every element length, number of elements and other datatype specific data to networkOrder
//BUT: the data itself remains untouched and thus networkOrder conversion for data needs to be taken care of by the user.
//Returns -1 on error;
int pckDataToNetworkOrder(unsigned char *pckData){
    if(pckData==NULL){
        return -1;
    }
    //Total len
    uint32_t totalLen = *(uint32_t*)pckData;
    *(uint32_t*)pckData = htonl(totalLen);
    //Pck increments (unnecessary)
    *(uint32_t*)(pckData+4) = htonl(*(uint32_t*)(pckData+4));

    //Loop every element and convert its len to net order
    uint32_t currentLen = 8;
    uint32_t currentElementLen = 0;
    while(currentLen<totalLen){
        currentElementLen = *(uint32_t*)(pckData+currentLen);
        *(uint32_t*)(pckData+currentLen) = htonl(currentElementLen);
        currentLen += (currentElementLen+4);
    }
    return 0;
}

//Converts every element length, number of elements and other datatype specific data to hostOrder
//BUT: the data itself remains untouched and thus hostOrder conversion for data needs to be taken care of by the user.
//Returns -1 on error;
int pckDataToHostOrder(unsigned char *pckData){
    if(pckData==NULL){
        return -1;
    }
    //Total len
    uint32_t totalLen = ntohl(*(uint32_t*)pckData);
    *(uint32_t*)pckData = totalLen;
    //Pck increments (unnecessary)
    *(uint32_t*)(pckData+4) = htonl(*(uint32_t*)(pckData+4));

    //Loop every element and convert its len to net order
    uint32_t currentLen = 8;
    uint32_t currentElementLen = 0;
    while(currentLen<totalLen){
        currentElementLen = ntohl(*(uint32_t*)(pckData+currentLen));
        *(uint32_t*)(pckData+currentLen) = currentElementLen;
        currentLen += (currentElementLen+4);
    }
    return 0;
}

//Returns the decrypted packet (including the nonce, packetGSettings and data itself).
//mbedtls_gcm_context *ctx - pointer to ctx, contains AES-GCM decryption key. Declate with initGCM()
//*encryptedMsg - pointer to encrypted message (points to beginning of msg so includes non encrypted part of msg as well)
//*outputBuf - a pointer to output the DECRYPTED part of message to. So it will contain nonce, packetGSettings and decrypted data itself. The reason for passing entire message to function is to get IV and verify AAD data in AES-GCM.
//IMPORTANT: OUTPUT BUFFER CANNOT BE SAME AS INPUT BUFFER (@@@add error check for this)
//Returns 0 on success, -1 on error.
int decryptPckData(mbedtls_gcm_context *ctx, unsigned char *encryptedMsg, unsigned char *outputBuf){
    //Plaintext data
    print2("ENCRYPTED MSG:",encryptedMsg,255,0);
    unsigned char *msgLenPtr = encryptedMsg;
    unsigned char *addLenPtr = msgLenPtr+4;
    unsigned char *extraDataLenPtr = addLenPtr+4;
    //Get length values
    uint32_t msgLen = ntohl(*(uint32_t*)msgLenPtr);
    uint32_t addLen = ntohl(*(uint32_t*)addLenPtr);
    uint32_t extraDataLen = ntohl(*(uint32_t*)extraDataLenPtr);
    printf("msgLen: %d\n",msgLen);
    printf("addLen: %d\n",addLen);
    printf("extraDataLen: %d\n",extraDataLen);
    unsigned char *ivPtr = extraDataLenPtr+4;
    unsigned char *tagPtr = ivPtr+IVLEN;
    unsigned char *addPtr = tagPtr+TAGLEN;
    unsigned char *addPckDataPtr = addPtr+protocolSpecificAddLen;
    unsigned char *extraDataPtr = addPtr+addLen;
    unsigned char *extraDataPckPtr = extraDataPtr+protocolSpecificExtraDataLen;
    unsigned char *encryptDataPtr = extraDataPtr+extraDataLen;
    uint32_t eDataLen = msgLen - (4 + 4 + 4 +IVLEN + TAGLEN + addLen);
    print2("edata:",encryptDataPtr,eDataLen,0);
    printf("eDataLen: %d\n",eDataLen);
    //Encrypted data will be handled after decryption in separate buffer cause we cant decrypt and output data in same buffer, unlike with encryption

    //Decrypt
    if(addLen==0){
        addPtr = NULL;
    }
    if(decryptGcm(ctx,encryptDataPtr,eDataLen,ivPtr,IVLEN,addPtr,addLen,tagPtr,TAGLEN,outputBuf)!=0){
        printf("Failed decrypting data\n");
        return -1;
    }
    unsigned char *pckDataPtr = outputBuf+4+4;
    //Deserialize data
    *(uint32_t*)outputBuf = ntohl(*(uint32_t*)outputBuf);  //Nonce
    *(uint32_t*)(outputBuf+4) = ntohl(*(uint32_t*)(outputBuf+4));  //PckGSettings
    pckDataToHostOrder(pckDataPtr);
    if((addLen-protocolSpecificAddLen)>0){
        pckDataToHostOrder(addPckDataPtr);
    }
    print2("Output buffer decrypted data:",outputBuf,eDataLen,0);
    return 0;

}

//Advanced print(). Good for debugging binary data, prints labelMsg[255] before the data itself
//datatype = 0: prints bytes in integer form
//datatype = 1: prints bytes in character form
//datatype = 2: prints 32 bit numbers
void print2(char labelMsg[255], unsigned char *data, int length, int datatype){
    int increments = 1;
    //data = unsigned char (numbers)
    if(datatype==0){
        increments = 1;
    }
    //data = unsigned char (letters)
    else if(datatype==1){
        increments = 1;
    }
    //data = uit32_t (numbers)
    else if(datatype==2){
        increments = 4;
    }

    printf("%s\n",labelMsg);
    for(int i =0; i<length;i+=increments){
        if(datatype==0){
            printf("%d ",*(data+i));
        }else if(datatype==1){
            printf("%c ",*(data+i));
        }else if(datatype==2){
            printf("%d ", *(uint32_t*)(data+i));
        }
    }
    printf("\n");
}

//Gives you a pointer to addData for the msgPtr provided
//IMPORTANT: For add to be verified first decrypt the message and check that tag was correct(means add wasnt modified by 3rd party). If integrity isnt important
//you can use it directly on recieved message
unsigned char* getPointerToUserAddData(unsigned char *msgPtr){
    unsigned char *addPtr = msgPtr + 4 + 4 + 4 + IVLEN + TAGLEN + protocolSpecificAddLen;
    return addPtr;
}

//Reads addData from message and assigns every addPckDataElement pointer to arrayList addDataPointers.
//IMPORTANT: USE AFTER MSG WAS DECRYPTED TO VERIFY THAT IT WASNT MODIFIED BY 3RD PARTY (If it were, the tag would be incorrect and decryption will fail!)
//NOTE: Use before decryption only if completely necessary or for unsensitive info (such as for devId, because you need to know it for decryption key)
int readAddData(unsigned char *msgPtr, arrayList *addDataPointers){
    //Read only user defined add, ignore stuff such as versionNum and anything else protocol defined
    initList(addDataPointers,4+sizeof(unsigned char*));
    unsigned char *msgAddPtr = msgPtr + 4 + 4 + 4 + IVLEN + TAGLEN + protocolSpecificAddLen;
    uint32_t pckDataLen = *(uint32_t*)msgAddPtr;
    uint32_t currentLen = 8;  //4(pckDataLen) + 4(pckDataIncrements)
    uint32_t currentElemLen = 0;
    unsigned char *currentDataPtr = NULL;
    while(currentLen<pckDataLen){
        printf("current len: %d\n",currentLen);
        unsigned char elementData[4+sizeof(unsigned char*)];
        currentElemLen = *(uint32_t*)(msgAddPtr+currentLen);
        *(uint32_t*)elementData = currentElemLen;
        printf("element len: %d\n",currentElemLen);
        unsigned char *addEntryPtr = msgAddPtr+currentLen+4;
        memcpy(elementData+4,&addEntryPtr, sizeof(unsigned char *));
        print2("Current add element data:",elementData,12,0);
        addToList(addDataPointers,elementData);
        currentLen+=(currentElemLen+4);
    }
    return 0;
}

//Reads extra data and assigns every pckData element pointer into an arrayList extraDataPointers. ExtraDdata is used for non-encrypted data that needs not be verified(so can be modified by 3rd party)
int readExtraData(unsigned char *msgPtr, uint32_t addLen, arrayList *extraDataPointers){
    int datasize = 4 + sizeof(unsigned char*);
    initList(extraDataPointers,datasize);
    unsigned char *msgExtraDataPtr = msgPtr + 4 + 4 + 4 + IVLEN + TAGLEN + addLen + protocolSpecificExtraDataLen;
    uint32_t pckDataLen = *(uint32_t*)msgExtraDataPtr;
    uint32_t currentLen = 8;  //4(pckDataLen) + 4(pckDataIncrements)
    uint32_t currentElemLen = 0;
    unsigned char *currentDataPtr = NULL;
    while(currentLen<pckDataLen){
        printf("current len: %d\n",currentLen);
        unsigned char elementData[4+sizeof(unsigned char*)];
        currentElemLen = *(uint32_t*)(msgExtraDataPtr+currentLen);
        *(uint32_t*)elementData = currentElemLen;
        printf("element len: %d\n",currentElemLen);
        unsigned char *extraDataEntryPtr = msgExtraDataPtr+currentLen+4;
        memcpy(elementData+4,&extraDataEntryPtr, sizeof(unsigned char *));
        print2("Current add element data:",elementData,12,0);
        addToList(extraDataPointers,elementData);
        currentLen+=(currentElemLen+4);
    }
    return 0;
}

//Read decrypted pckData and assigns a pointer to every pckData element into decrypedDatapointers arrayList
int readDecryptedData(unsigned char *decryptedData, arrayList *decryptedDataPointers){
    uint32_t totalLen = 16;  //4(nonce) + 4(pckGSettings) + 4(pckDataLenTotal) + 4(pckDataIncrements)
    uint32_t decryptedDataLen = *(uint32_t*)(decryptedData+8);
    initList(decryptedDataPointers,(sizeof(unsigned char*)+4));  //Enough to store length and data pointer
    //Read all elements
    uint32_t currentElementLen = 0;
    while(totalLen<decryptedDataLen){
        currentElementLen = *(uint32_t*)(decryptedData+totalLen);
        printf("current element len: %d. Total len: %d\n",currentElementLen,totalLen);
        //Assign element data to arrayList
        unsigned char elementData[sizeof(unsigned char *)+4];
        *(uint32_t*)elementData = currentElementLen;
        unsigned char *tempDataPtr;
        unsigned char *tempSourcePtr = decryptedData+totalLen+4;
        memcpy(&tempDataPtr,&tempSourcePtr,sizeof(unsigned char*));
        memcpy((elementData+4),&tempDataPtr,sizeof(unsigned char*));
        print2("ADDING ELEMENT TO LIST:",elementData,12,0);
        addToList(decryptedDataPointers,elementData);
        totalLen += (4+currentElementLen);
    }
    return 0;
}

uint32_t getNonceFromDecryptedData(unsigned char *decryptedDataPtr){
    return *(uint32_t*)decryptedDataPtr;
}

uint32_t getPckGSettingsFromDecryptedData(unsigned char *decryptedDataPtr){
    return *(uint32_t*)(decryptedDataPtr+4);
}

//Gets a pckData element from pointerToData at specific index, spitting out its length as return value and pointer to element itself is assigned to *ptrToElement
//Returns elements length (or -1 on error)
uint32_t getElementFromPckData(arrayList *pointersToData, unsigned char **ptrToElement, int index){
    unsigned char *tempElementPtr = getFromList(pointersToData,index);
    unsigned char *dataPtr;
    memcpy(&dataPtr,(tempElementPtr+4),sizeof(unsigned char*));
    if(tempElementPtr==NULL){
        printf("Wrong index in pckData!\n");
        return -1;
    }
    print2("Got element from list:",tempElementPtr,12,0);
    //print2("Pointer:",&dataPtr,10,0);
    //print2("Pointer dereferenced:",dataPtr,8,0);
    //Assign pointer
    *ptrToElement = dataPtr;
    //Return element length
    return *(uint32_t*)(tempElementPtr);
}

//Recieves messages and processes them with a corresponding processing function. process buf(3rd argument must be of at least max msg length size)
//arrayList *recvHoldersList - a list of recvHolders. Inits during initBasicServerData. Keeps recvHolders allowing us to recieve defragmented TCP messages. Automatically cleans them up when not in use.
//arrayList *connInfos - a list of connInfos. Initialized during setup and populated by connect1() and accept1()
//socket - socket to recieve data from.
//*processBuf - a buffer to which the message gets stored to. Once full message is recieved the callback function should access this buffer to get the message. The first 4 bytes of every message is its length in uint32_t in network order. Buffer gets overriden after every new function call. So use separated buffers in multithreaded environments
//processingCallback - a function pointer, this function gets called after a complete message is recieved and is the final message (in its raw form, encrypted and stuff)
int recvAll(arrayList *recvHoldersList, arrayList *connInfos, int socket, unsigned char *processBuf, processingCallback processingMsgCallback){
    printf("recvAll started\n");

    //Get connInfo(created during connect1() or accept1()) for this socket
    connInfo_t *connInfo = findConnInfo(socket,connInfos);
    if(connInfo==NULL){

        printf("No connInfo found, did u use connect1()?\n");
        return -1;
    }
    //recvHolder to use for this message and socket
    recvHolder *recvHolderToUse = NULL;

    //recvHolder index in recvHoldersList (knowing makes it easier to free the recvHolder from memory)
    int recvHolderToUseIndex = -1;

    //Check if any recvHolders are in use. If so loop and see if one belongs to this socket
    if(recvHoldersList->length>0){
        //Find if one belongs to this socket
        for(int i =0; i<recvHoldersList->length; i++){
            recvHolder *recvHolderTemp = getFromList(recvHoldersList,i);
            printf("Found recv queue in use for socket: %i\n",recvHolderTemp->socket);
            if(recvHolderTemp->socket==socket){
                //Found queue belonging to this socket!
                printf("Found queue belonging to this socket at index %i!\n",i);
                recvHolderToUse = recvHolderTemp;
                recvHolderToUseIndex = i;
            }
        }
    }
    unsigned char freeHolderFound = 0;
    //If this socket has no holder
    if(recvHolderToUse==NULL){
        printf("No associated holder found for this socket\n");
        //Check if any holders are free to use (socket field will be -1).
        for(int i = 0; i <recvHoldersList->length; i++){
            recvHolder *tempHolder = getFromList(recvHoldersList,i);
            if(tempHolder->socket==-1){
                //Found available free holder!
                printf("Found available free holder!\n");
                recvHolderToUse = tempHolder;
                recvHolderToUseIndex = i;
                freeHolderFound = 1;
                break;
            }
        }

        //If no free holder was found, create one
        if(freeHolderFound==0){
            //If no holders free. Create a new one
            printf("No free holders found. Creating new one\n");
            //Init new holder structure
            recvHolder newHolder;
            initRecvHolder(&newHolder,MAXDEVMSGLEN*3,socket);
            //Add new holder to the recvHolders list
            if(addToList(recvHoldersList,&newHolder)!=0){
                printf("Failed to add new queue to list\n");
                exit(1);
            }
            //Index is last in list because adding always adds to the end of list!
            recvHolderToUseIndex = recvHoldersList->length-1;
            if((recvHolderToUse = getFromList(recvHoldersList,recvHolderToUseIndex))==NULL){
                //Index non existent
                printf("Failed to get recvHolder from list at index %i. Index non-existent",recvHolderToUseIndex);
                exit(1);
            }
        }

    }

    //------HERE WE SHOULD HAVE A QUEUE. NOW JUST ANALYZE/MODIFY IT - (recvHolder *recvHolderToUse)
    //Reset time since used in this queue so it doesnt get removed due to timeout
    recvHolderToUse->timeSinceUsed = 0;
    //printf("Queue obtained\n");

    int rs;
    do{
        printf("Looping recv loop\n");
        rs = recv(socket,recvHolderToUse->recvQueue+recvHolderToUse->sizeUsed,(recvHolderToUse->size-recvHolderToUse->sizeUsed),0);
        printf("Recieved: %i bytes\n",rs);
        if(rs==-1){
            //Buffer over or error
            //printf("Buffer over or error occured\n");
            perror("recv = -1");
            return -1;
        }else if(rs==0){
            //Connection closed
            //printf("Connection closed\n");
            close(socket);

            //Set corresponding recvHolder's socket to -1 meaning other sockets can take it, it also clears recvHolder's fields
            resetRecvHolder(recvHolderToUse,-1);

            return 1;
        }
        //Got genuine message
        //printf("Recieved genuine message!\n");

        //------HANDLING MSG LENGTH
        //Only check length if it hasnt been previously assigned or was partially recieved previously
        //Check if we are recieving a completely new message(holder->msgPointer==NULL) or finishing off an older one(holder->msgPointer!=NULL).
        //or if length <4 meaning that length wasnt completely recieved on first try
        if(recvHolderToUse->firstMsgPtr==NULL || recvHolderToUse->firstMsgSize<4){
            handleMsgLen(recvHolderToUse,rs);
        }

        //------HANDLING CIRCULAR BUFFER AND SEEING IF MSG IS COMPLETE OR NOT
        recvHolderToUse->firstMsgSizeUsed += rs;
        handleMsgData(connInfo, recvHolderToUse, rs, processBuf, processingMsgCallback);


    }while(rs>0);
}

//Part of recvAll() function. Handles message length
int handleMsgLen(recvHolder *recvHolderToUse, int rs){
    printf("FINDING MSG LENGTH\n");
    //Assign pointer to start of msg if it is the very first bit of data sent from it
    if(recvHolderToUse->firstMsgSize==0 && recvHolderToUse->firstMsgSize!=5){
        printf("Assigning starting pointer to msg\n");
        recvHolderToUse->firstMsgPtr = (recvHolderToUse->recvQueue+recvHolderToUse->sizeUsed);
    }

    //------GETTING MESSAGE LENGTH
    //If partial length set msgSize to amount of bytes recieved to let the next time recv() is called know it and finish it off
    recvHolderToUse->firstMsgSize +=rs;

    //Once got 4 bytes(full length) assign them
    if(recvHolderToUse->firstMsgSize>=4){
        printf("Recieved full msg length!\n");
        recvHolderToUse->firstMsgSize = ntohl(*((uint32_t*)(recvHolderToUse->firstMsgPtr)));
        printf("Msg size: %i\n",recvHolderToUse->firstMsgSize);
    }
}

//Part of recvAll() function. Handles msgData processing.
//Process buf (3rd parameter) must be of size of max msg length
int handleMsgData(connInfo_t *connInfo, recvHolder *recvHolderToUse, int rs, unsigned char *processMsgBuffer, processingCallback processingMsgCallback){
    unsigned char hasBufferCirculated = 0;
    printf("First msg total size: %i/%i\n",recvHolderToUse->firstMsgSizeUsed,recvHolderToUse->firstMsgSize);

    //If message completed, copy to processBuf and call processing callback
    if((recvHolderToUse->firstMsgSizeUsed>=recvHolderToUse->firstMsgSize) && recvHolderToUse->firstMsgSize>4){
        //If first msg completely recieved

        //Check if message is overlapping the buffer
        ptrdiff_t tillBufferEnd = (int)((recvHolderToUse->recvQueue+recvHolderToUse->size-1)-(recvHolderToUse->firstMsgPtr+(recvHolderToUse->firstMsgSizeUsed)-1));

        if(tillBufferEnd<=0){
            //If message is overlapping buffer
            //printf("Copying overlapping message to process buffer\n");
            memcpy(processMsgBuffer,recvHolderToUse->firstMsgPtr,recvHolderToUse->firstMsgSize+tillBufferEnd);
            memcpy(processMsgBuffer+recvHolderToUse->firstMsgSize+tillBufferEnd,recvHolderToUse->recvQueue,-(tillBufferEnd));
        }else{
            //If message isnt overlapping buffer
            //printf("Copying non buffer overlapping message\n");
            memcpy(processMsgBuffer,recvHolderToUse->firstMsgPtr,recvHolderToUse->firstMsgSize);
        }


        //Call prcocessor method after message assigned to prcoessor buffer
        processingMsgCallback(connInfo, processMsgBuffer);

        recvHolderToUse->firstMsgPtr = NULL;



        //If some data recieved belonged to the second message, handle it here
        if(recvHolderToUse->firstMsgSizeUsed>(recvHolderToUse->firstMsgSize)){
            //If recieved some (or all) of the second message
            printf("Recieved some of the 2nd message\n");
            recvHolderToUse->firstMsgPtr = (recvHolderToUse->recvQueue+recvHolderToUse->sizeUsed+recvHolderToUse->firstMsgSize);
            recvHolderToUse->firstMsgSizeUsed = recvHolderToUse->firstMsgSizeUsed-recvHolderToUse->firstMsgSize;
            recvHolderToUse->firstMsgSize = 5;
            printf("%i bytes recieved of second msg\n",recvHolderToUse->firstMsgSizeUsed);

            //CIRCULATE BUFFER TO AVOID ERRORS IN HANDLING SECOND MSG AND SET hasBufferCirculated to true
            //to avoid further circulations
            circulateBuffer(recvHolderToUse,rs);
            hasBufferCirculated = 1;

            handleMsgLen(recvHolderToUse,recvHolderToUse->firstMsgSizeUsed);
            printf("Second msg size: %i\n",recvHolderToUse->firstMsgSize);
            handleMsgData(connInfo, recvHolderToUse, rs, processMsgBuffer,processingMsgCallback);
        }else{
            recvHolderToUse->firstMsgSize = 0;
            recvHolderToUse->firstMsgSizeUsed = 0;
        }


    }else{
        //Finishing older message
        //printf("Finishing older message\n");
    }

    //--CIRCULATE BUFFER (if it hasnt already been circulated due to recieving second msg)
    if(hasBufferCirculated==0){
        circulateBuffer(recvHolderToUse,rs);
    }
}


//part of recvAll() handles circulating the buffer
void circulateBuffer(recvHolder *recvHolderToUse, int rs){
    //------CREATE CIRCULAR BUFFER BEHAVIOUR
    recvHolderToUse->sizeUsed += rs;
    //printf("Size used: %i. Total size: %i\n",recvHolderToUse->sizeUsed,recvHolderToUse->size);
    if(recvHolderToUse->size==recvHolderToUse->sizeUsed){
        //printf("Overlapping buffer! Sizes are equal\n");
        recvHolderToUse->sizeUsed = 0;
    }
}

//RECV HOLDER HANDLING FUNCTIONS

//inits a new recvHolder. Should only be used inside recvAll()
int initRecvHolder(recvHolder *holder, int maxSize, int socket){
    printf("Initializing recvHolder\n");
    holder->socket = socket;
    holder->firstMsgPtr = NULL;
    holder->size = maxSize;
    holder->sizeUsed = 0;
    holder->firstMsgSize = 0;
    holder->firstMsgSize = 0;
    holder->firstMsgSize = 0;
    holder->firstMsgSizeUsed = 0;
    if((holder->recvQueue = malloc(holder->size))==NULL){
        perror("Failed to init holder");
        exit(1);
    }
}

//Sets all values in recvHolder to defaults. Should only be used in recvAll(). DOESNT DEALLOC THE recvHolder, only defaults its values
int resetRecvHolder(recvHolder *holder,int socket){
    printf("Reseting recvHolder\n");
    holder->socket = socket;
    holder->firstMsgPtr = NULL;
    holder->sizeUsed = 0;
    holder->firstMsgSize = 0;
    holder->firstMsgSizeUsed = 0;
}

//Deallocs the recvHolder
int removeRecvHolder(arrayList *recvHolders, int index){
    //Free allocated *recvQueue pointer inside recvHolder
    recvHolder *tempRecvHolder;
    if((tempRecvHolder = getFromList(recvHolders,index))==NULL){
        printf("Inexistent index provided to removeRecvQueue function!!\n");
        exit(1);
    }
    free(tempRecvHolder->recvQueue);

    //Remove recvHolder itself from recvHolders
    removeFromList(recvHolders,index);
}

//Modification of send() syscall. Sends everything over unlike send()
int sendall(int s, unsigned char *buf, int len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = len; // how many we have left to send
    int n;

    while(total < len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}

//Sends a part of sessionId. Both communicating nodes are required to do this
int sendPartOfSessionId(arrayList *connInfos, int socket){
    //Generated new random number
    uint32_t partOfSessionId;
    getrandom(&partOfSessionId,4,0);


}

//Recieve part of sessionId. Both communicating nodes are required to do this
int recievePartOfSessionId(arrayList *connInfos, arrayList *recvHolders, int socket, unsigned char *processingBuffer){
    printf("Recieving part of sessionId....\n");
    recvAll(recvHolders,connInfos, socket,processingBuffer,internalRecievePartOfSessionId);
}

static int internalRecievePartOfSessionId(connInfo_t *connInfo, unsigned char *processingBuf){
    if(connInfo==NULL){
        return -1;
    }
    printf("Recieved part of SessionId from socket: %d\n", connInfo->socket);
}

connInfo_t* findConnInfo(int socket, arrayList *connInfosList){
    //Find conn info for this socket
    printf("Fuckkk");
}


//int initializeSettingsData(globalSettingsStruct *globals){
//    printf("Initializing settings\n");
//    //Initialize defaults, in case not found in settings file
//    globals->passExchangeMethod = 1; //Use master password by default
//
//    unsigned char temp[40];
//    FILE *settingsFile;
//    int currentCharacters;
//    unsigned char *settingPtr;
//    char isSettingPtrComplete = 0;
//    int tempSettingLen;
//    uint32_t currentOption;
//
//    if((settingsFile=fopen(settingsFileName,"r"))==NULL){
//        printf("Failed opening settings file\n");
//        return -1;
//    }
//
//    while(fgets(temp,60,settingsFile)!=NULL){
//        currentCharacters = strcspn(temp,"\n");
//        settingPtr = temp;
//        for(int i = 0; i<currentCharacters;i++){
//            if((*(temp+i)==' ' || *(temp+i)==':') && isSettingPtrComplete == 0 ){
//                //Completely got the setting name
//                tempSettingLen = i;
//                isSettingPtrComplete = 1;
//            }else if(isSettingPtrComplete==1 && (*(temp+i)!=' ' || *(temp+i)!=':')){
//                currentOption = atoi(temp+i);
//                isSettingPtrComplete = 0;
//                break;
//            }
//        }
//        //printf("%s\n",settingPtr);
//        if(memcmp(settingPtr,"passExchangeMethod",18)==0){
//            //PASS EXCHANGE OPTION
//            printf("Setting passExchangeMethod to %d\n",currentOption);
//            globals->passExchangeMethod = currentOption;
//        }
//
//
//    }
//
//}



int initializeSettingsData(globalSettingsStruct *globalSettings){
    //Open the file
    FILE *settingsFile;
	char temp[40];
	int currentCharacters;


    if((settingsFile = fopen(settingsFileName,"r"))==NULL){
        printf("Failed opening node settings file\n");
        return -1;
    }
    printf("Settings loaded are:\n");

    //Analyze every line
	while(fgets(temp,60,settingsFile)!=NULL){
		currentCharacters = strcspn(temp,"\n");
		char *settingPtr = temp;
        char *settingName;
        char *settingValue;
        char *settingNameBegin;
        char *valueNameBegin;
        int settingsNamePassedFirstQuote = 0;
        int settingNameComplete = 0;
        char *settingValueBegin;
        int settingValuePassedFirstQuote = 0;
        int settingValueComplete = 0;


		for(int i = 0; i<currentCharacters;i++){
            if(*(temp+i)=='"' && settingsNamePassedFirstQuote==1 && settingNameComplete!=1){
                //Malloc space for setting name (and later value), remember to dealloc
                int settingNameLen =(temp+i)-settingNameBegin-1;
                settingName = malloc(settingNameLen);
                memcpy(settingName, settingNameBegin+1, settingNameLen);
                *(settingName+settingNameLen) = 0;
                settingNameComplete = 1;
                
            }else if(*(temp+i)=='"' && settingsNamePassedFirstQuote==0 && settingNameComplete!=1){
                settingNameBegin = temp+i;
                settingsNamePassedFirstQuote=1;
            }else if(*(temp+i)=='"' && settingValuePassedFirstQuote==1 && settingNameComplete==1){
                int settingValueLen = (temp+i)-settingValueBegin-1;
                settingValue = malloc(settingValueLen);
                memcpy(settingValue, settingValueBegin+1, settingValueLen);
                *(settingValue+settingValueLen) = 0;
                settingValueComplete = 1;
            }else if(*(temp+i) == '"' && settingValuePassedFirstQuote == 0 && settingNameComplete==1){
                settingValueBegin = temp+i;
                settingValuePassedFirstQuote = 1;
            }

            if(settingValueComplete==1){
                break;
            }
            
        }
        printf("NAME: %s\n",settingName);
        printf("VALUE: %s\n",settingValue);
        //Analyze the obtained setting name and values and dealloc
        if(strcmp(settingName,"passExchangeMethod")){
            globalSettings->passExchangeMethod = atoi(settingValue);
        }
        free(settingName);
        free(settingValue);
    }
    return 1;
}

//Modifies a setting of settingName inside the config file. Hence for the change to apply settings need to be reloaded
int modifySetting(unsigned char *settingName, int settingLen, uint32_t newOption){
	//Find settingName in settings.conf file in same directory
	FILE *settingsFile;
	FILE *tempSettingsFile;
	char tempFileName[40];
	strcpy(tempFileName,settingsFileName);
	strcat(tempFileName,".temp");
	printf("%s\n",tempFileName);
	if((settingsFile = fopen(settingsFileName,"r"))==NULL){
		perror("Failure opening settings file");
		return -1;
	}
	if((tempSettingsFile = fopen(tempFileName,"w"))==NULL){
		perror("Failure generating temp settings file\n");
		return -1;
	}
	unsigned char temp[30];
	int lastPos;
	int currentCharacters;
	unsigned char *settingPtr;
	char isSettingPtrComplete = 0;
	uint32_t tempSettingLen;
	int currentOption;
    int optionFound = 0;
	while(fgets(temp,60,settingsFile)!=NULL){
		currentCharacters = strcspn(temp,"\n");
		lastPos+=currentCharacters;
		settingPtr = temp;
		for(int i = 0; i<currentCharacters;i++){
			if((*(temp+i)==' ' || *(temp+i)==':') && isSettingPtrComplete == 0 ){
				//Completely got the setting name
				tempSettingLen = i;
				isSettingPtrComplete = 1;
			}else if(isSettingPtrComplete==1 && (*(temp+i)!=' ' || *(temp+i)!=':')){
				currentOption = atoi(temp+i);
				isSettingPtrComplete = 0;
				break;
			}
		}
		if(memcmp(settingName,settingPtr,settingLen)==0){
			//printf("This setting found\n");
			//Move to beginning of previous line
			lastPos -= currentCharacters;
			//printf("Last pos: %d\n",lastPos);
			fprintf(tempSettingsFile,"%s:%d\n",settingName,newOption);
            optionFound = 1;
		}else{
			//If not needed option just copy it to temp file
			//printf("no data but copying\n");
			//printf("%s\n",temp);
			fputs(temp,tempSettingsFile);
		}
	}
    if(optionFound!=1){
        //This option doesnt exist in file. Create it.
        fprintf(tempSettingsFile, "%s:%d\n", settingName, newOption);
    }
	//Check if error occured
	if(ferror(tempSettingsFile)!=0 || ferror(settingsFile)!=0){
		//Error. Don't copy
		return -2;
	}else{
		//All good
		rename(tempFileName,settingsFileName);
	}

	fclose(settingsFile);
	fclose(tempSettingsFile);
}

