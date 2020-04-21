/* Main server for esp32 connected IoT systems
 */

#define _GNU_SOURCE

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
#include "../../misc/mbedtls/include/mbedtls/gcm.h"
#include "../../misc/ArrayList/arrayList.c"
#include "../../misc/AES-GCM/aes-gcm.h"
#include "server.h"
#include "commandHandlesServer.h"
#include "pthread.h"
#include "timingThread.h"
#include "controlSocketThread.h"
#include "../../protocol/pckData/pckData.h"

//FILES LOCATIONS
unsigned char settingsFileName[30] = "settings.conf";

//MACROS
#define MAXEVENTS 255

//MULTITHREADING
pthread_t timingThreadID;
pthread_t controlSockThreadID;
pthread_mutex_t recvHolderMutex;
pthread_mutex_t commandQueueAccessMux;
int counter = 0;

//DevId to key. First 12 bytes are device id. Next 16 bytes are the key. Total size = 28 bytes
arrayList devInfos;
//Needed for correct operation of recvAll func
recvHolder recvQueue;
arrayList recvHolders;
//Global settings
globalSettingsStruct globalSettings;

//Dictionary of socket to connInfo
arrayList connectionsInfo;
arrayList commandsQueue;

//Every ready message is copied here and then immediately processed away by processMsg()
unsigned char processMsgBuffer[MAXDEVMSGLEN];
unsigned char decryptedProcessMsgBuffer[MAXDEVMSGLEN];
int socketBeingProcessed = -1;

int main(void){
	//Initialization
    initBasicServerData(&connectionsInfo, &recvHolders, &devInfos, &commandsQueue, &globalSettings, 0);
	modifySetting("passExchangeMethod",18,0);

	//TEST COMMAND
	unsigned char tempDevId[12] = "DevId1234567";
	unsigned char tempKey[16] = "KeyKeyKey1234567";
	uint32_t tempOpCode = 0;
	unsigned char tempGSettings = 0;
	uint32_t tempArgsLen = 10;
	unsigned char tempCmdArgs[10] = "Oi big man";
	addToCmdQueue(&commandsQueue,tempDevId,tempOpCode,tempGSettings,tempCmdArgs,tempArgsLen,NULL,0);

	//Multithreading stuff
	pthread_mutex_init(&recvHolderMutex,NULL);
	pthread_mutex_init(&commandQueueAccessMux,NULL);
	pthread_create(&timingThreadID,NULL,timingThread,NULL);
	pthread_create(&controlSockThreadID,NULL,controlSockThread,NULL);

	devInfo tempDevInfo;
	memcpy(tempDevInfo.devId, tempDevId, DEVIDLEN);
	memcpy(tempDevInfo.key, tempKey, KEYLEN);
	tempDevInfo.devType = 1;
	if(addToList(&devInfos,&tempDevInfo)!=0){
		printf("Failed adding to DevId list\n");
	}

	//General variables
	int sockLocal, sockRemote;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage remote_addr;  //Remote addr info
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char connAddr[INET_ADDRSTRLEN];
	int rv;
	unsigned char msgBuf[48];

	//epoll variables
	int epollMain;
	struct epoll_event ev,events[MAXEVENTS];

	//Clear up hints structure before using it
	memset(&hints,0,sizeof(hints));

	//Local socket params
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;  //Uses local ip

	//Get addr info
	char mainInPortStr[10];
	sprintf(mainInPortStr,"%d",mainInPort);
	if((rv = getaddrinfo(NULL, mainInPortStr, &hints, &servinfo)) != 0){
		fprintf(stderr, "getaddrinfo %s\n",gai_strerror(rv));
		return 1;
	}

	//Loop through addr info results
	for(p = servinfo; p!=NULL; p->ai_next){
		if((sockLocal = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1){
			//Error setting socket
			perror("Server: Socket");
			exit(1);
		}

		//Make socket reusable instantly (because removed socket might still occupy ports for some time, this instructs to ignore them)
		if(setsockopt(sockLocal,SOL_SOCKET,SO_REUSEADDR, &yes, sizeof(int)) == -1){
			perror("setsockopt:");
			exit(1);
		}

		//Bind socket
		if(bind(sockLocal,p->ai_addr,p->ai_addrlen) == -1){
			close(sockLocal);
			perror("Server: Bind");
			exit(1);
		}

		break;
	}

	//No longer need addrinfo struct
	freeaddrinfo(servinfo);

	//Check if found any usable addr
	if(p == NULL){
		fprintf(stderr, "Server: failed to bind\n");
		exit(1);
	}

	//Start listening
	if(listen(sockLocal,maxConcurentConnectionsToMainSocket) == -1){
		perror("Server: Listen");
		exit(1);
	}

	//Initialize epollMain
	epollMain = epoll_create1(0);

	if(epollMain==-1){
		perror("epoll_create1() failed");
		exit(1);
	}

	ev.events = EPOLLIN;
	ev.data.fd = sockLocal;

	//Add listening socket to epollMain
	if(epoll_ctl(epollMain,EPOLL_CTL_ADD,sockLocal,&ev)==-1){
		perror("Epoll_ctl() of sockLocal failed");
		exit(1);
	}

	printf("Server: Initialized epoll instance\n");
	printf("Server: waiting for connections...\n");

	//Main accept loop
	while(1)
	{

		int totalEvents = epoll_wait(epollMain,events,MAXEVENTS,-1);
		printf("Server: waiting for new connections\n");

		//IF ANY EVENTS HAPPENED
		if(totalEvents>0)
		{
			//Loop through all events
			for(int i =0; i<totalEvents; i++)
			{
				socketBeingProcessed = events[i].data.fd;
				//Got new connection
				if(events[i].data.fd==sockLocal)
				{
					//Accept new connections
					sin_size = sizeof(remote_addr);
					sockRemote = accept1(sockLocal,(struct sockaddr *)&remote_addr, &sin_size, &connectionsInfo);
						perror("socket accepting failed");
						exit(1);
					}
					//Get ip string for new connection
					inet_ntop(AF_INET, &(((struct sockaddr_in*)&remote_addr)->sin_addr),connAddr,INET_ADDRSTRLEN);
					printf("Accepting new connection at %s\n", connAddr);
					//Set new connection as non blocking and add to epoll as EPOLLIN
					//Set non blocking
					if(fcntl(sockRemote,F_SETFL,O_NONBLOCK)==-1)
					{
						perror("Failed setting new socket non blocking");
						exit(1);
					}
					//Add to epoll
					ev.events = EPOLLIN;
					ev.data.fd = sockRemote;
					if(epoll_ctl(epollMain, EPOLL_CTL_ADD,sockRemote,&ev)==-1)
					{
						perror("epoll fcntl for new connection failed");
						exit(1);
					}
					//Conn info. Neccesary for msg nonces
					printf("Creating a connInfo for socket: %d\n",sockRemote);
					addConnInfo(sockRemote,-1,-1);

					printf("Succesfully accepted/configured new connection\n");

				}
				//Recieve message from any of connected sockets
				else if(events[i].data.fd!=sockLocal && events[i].events==EPOLLIN)
				{
					printf("Recieved new message!\n");
					//LOCK MUTEX
					pthread_mutex_lock(&recvHolderMutex);
					recvAll(&recvHolders,events[i].data.fd,processMsgBuffer,processMsg);
					pthread_mutex_unlock(&recvHolderMutex);
				}
			}
		}
	}
}

/*
int initRecvHolder(recvHolder *holder, int maxSize, int socket){
	printf("Initializing recvHolder\n");
	holder->socket = socket;
	holder->firstMsgPtr = NULL;
	holder->size = maxSize;
	holder->sizeUsed = 0;
	holder->firstMsgSize = 0;
	holder->firstMsgSizeUsed = 0;
	if((holder->recvQueue = malloc(holder->size))==NULL){
		perror("Failed to init holder");
		exit(1);
	}
}

int resetRecvHolder(recvHolder *holder,int socket){
	printf("Reseting recvHolder\n");
	holder->socket = socket;
	holder->firstMsgPtr = NULL;
	holder->sizeUsed = 0;
	holder->firstMsgSize = 0;
	holder->firstMsgSizeUsed = 0;
}*/

//Allows for recieving asynchronous messages with temporary buffers for any incomplete message. Amount of consecutive
//buffers scales with usage. Any unused buffers (broken connection, DOS/DDOS attack, network problems) get removed after a predefined timeout
/*
int recvAll2(arrayList *recvHoldersList, int socket){
	printf("recvAll2 started\n");
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
			closeSocket(socket);

			//Set corresponding recvHolder's socket to -1 meaning other sockets can take it, it also clears recvHolder's fields
			resetRecvHolder(recvHolderToUse,-1);

			return 1;
		}
		//Got genuine message
		//printf("Recieved genuine message!\n");

		//------HANDLING MSG LENGTH
		//Only check length if it hasnt been previously assigned or was partially recieved previously
		//Check if we are recieving a completely new message(holder->msgPointer==NULL) or finishing off an older one(holder->msgPointer!=NULL).
		//or if length == -1 meaning that length wasnt completely recieved on first try
		if(recvHolderToUse->firstMsgPtr==NULL || recvHolderToUse->firstMsgSize==-1){
			handleMsgLen(recvHolderToUse,rs);
		}

		//------HANDLING CIRCULAR BUFFER AND SEEING IF MSG IS COMPLETE OR NOT
		recvHolderToUse->firstMsgSizeUsed += rs;
		handleMsgData(recvHolderToUse, rs);


	}while(rs>0);
}

int handleMsgLen(recvHolder *recvHolderToUse, int rs){
	//printf("FINDING MSG LENGTH\n");
	if(recvHolderToUse->firstMsgSize!=-1 && recvHolderToUse->firstMsgSize!=-2){
		recvHolderToUse->firstMsgPtr = (recvHolderToUse->recvQueue+recvHolderToUse->sizeUsed);
	}

	//------GETTING MESSAGE LENGTH
	//Full length
	if(rs>1 && recvHolderToUse->firstMsgSize!=-1){
		//Length recieved!
		//printf("Full message length recieved\n");
		recvHolderToUse->firstMsgSize = ntohs(*((uint16_t*)(recvHolderToUse->firstMsgPtr)));
		//printf("Msg size: %i\n",recvHolderToUse->firstMsgSize);
	}
	//Partial length
	else if(rs==1 || recvHolderToUse->firstMsgSize==-1){
		//If partial length set msgSize to -1 to let the next time recv() is called know it
		//printf("Partial msg length recieved\n");
		if(recvHolderToUse->firstMsgSize==-1){
			//printf("Recieved second half of msg length!\n");
			recvHolderToUse->firstMsgSize = ntohs(*((uint16_t*)(recvHolderToUse->firstMsgPtr)));
			//printf("Msg size: %i\n",recvHolderToUse->firstMsgSize);
		}else{
			//printf("Recieved first part of partial msg legnth\n");
			recvHolderToUse->firstMsgSize = -1;
		}
	}
}

int handleMsgData(recvHolder *recvHolderToUse, int rs){
	unsigned char hasBufferCirculated = 0;
	//printf("HANDLING MSG DATA\n");
	printf("First msg total size: %i/%i\n",recvHolderToUse->firstMsgSizeUsed,recvHolderToUse->firstMsgSize);
	if((recvHolderToUse->firstMsgSizeUsed>=recvHolderToUse->firstMsgSize) && recvHolderToUse->firstMsgSize!=-1){
		//If first msg completely recieved

		//Check if message is overlapping the buffer
		ptrdiff_t tillBufferEnd = (int)((recvHolderToUse->recvQueue+recvHolderToUse->size-1)-(recvHolderToUse->firstMsgPtr+(recvHolderToUse->firstMsgSizeUsed)-1));
		//printf("Till buffer end: %i\n", tillBufferEnd);
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
		processMsg();

		recvHolderToUse->firstMsgPtr = NULL;



		//If some data recieved belonged to the second message, handle it here
		if(recvHolderToUse->firstMsgSizeUsed>(recvHolderToUse->firstMsgSize)){
			//If recieved some (or all) of the second message
			//printf("Recieved some of the 2nd message\n");
			recvHolderToUse->firstMsgPtr = (recvHolderToUse->recvQueue+recvHolderToUse->sizeUsed+recvHolderToUse->firstMsgSize);
			recvHolderToUse->firstMsgSizeUsed = recvHolderToUse->firstMsgSizeUsed-recvHolderToUse->firstMsgSize;
			recvHolderToUse->firstMsgSize = -2;
			//printf("%i bytes recieved of second msg\n",recvHolderToUse->firstMsgSizeUsed);

			//CIRCULATE BUFFER TO AVOID ERRORS IN HANDLING SECOND MSG AND SET hasBufferCirculated to true
			//to avoid further circulations
			circulateBuffer(recvHolderToUse,rs);
			hasBufferCirculated = 1;

			handleMsgLen(recvHolderToUse,recvHolderToUse->firstMsgSizeUsed);
			//printf("Second msg size: %i\n",recvHolderToUse->firstMsgSize);
			handleMsgData(recvHolderToUse, rs);
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

void circulateBuffer(recvHolder *recvHolderToUse, int rs){
	//------CREATE CIRCULAR BUFFER BEHAVIOUR
	recvHolderToUse->sizeUsed += rs;
	//printf("Size used: %i. Total size: %i\n",recvHolderToUse->sizeUsed,recvHolderToUse->size);
	if(recvHolderToUse->size==recvHolderToUse->sizeUsed){
		//printf("Overlapping buffer! Sizes are equal\n");
		recvHolderToUse->sizeUsed = 0;
	}
}

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
*/
//MESSAGE PROCESSING CODE

//Process the message placed in processMsgBuffer
//Returns 0 on sucess; 1 if msg has a terminator byte set (in confByte); -1 on error
int processMsg(){
	//Process message
	printf("PROCESSING MSG\n");
	//Get lengths
	uint32_t msgLen = ntohl(*(uint32_t*)processMsgBuffer);
	uint32_t addLen = ntohl(*(uint32_t*)(processMsgBuffer+4));
	printf("Msg len: %i\n", msgLen);
	printf("Add len: %i\n",addLen);

	//Get devId(first element in add) and find corresponding decryption key
	//Convert add to host order, get devId, convert back to network order(for decryption to work) and
	//decrypt, verifying the tag to make sure add wasnt modified by 3rd party
	arrayList pointersToAddData;
	unsigned char *userAddPckData = getPointerToUserAddData(processMsgBuffer);
	pckDataToHostOrder(userAddPckData);
	readAddData(processMsgBuffer,&pointersToAddData);
	unsigned char *devId;
	getElementFromPckData(&pointersToAddData,&devId,0);
	print2("DEVID FROM ADD:",devId,DEVIDLEN,1);

	//Find encryption key corresponding to this devId
	int devIdIndex = -1;
	//printList(&devIdToKey,1);
	printf("dev len: %d\n",devInfos.length);
	for(int i = 0; i<devInfos.length; i++){
		devInfo *tempInfo = getFromList(&devInfos,i);
		printf("Dev info found:\n");
		for(int i = 0; i<DEVIDLEN;i++){
			printf("%c",*(tempInfo->devId+i));
		}
		if(memcmp(devId,tempInfo->devId,DEVIDLEN)==0){
			devIdIndex = i;
			break;
		}
	}
	if(devIdIndex==-1){
		printf("No devInfo found for this device\n");
		return -1;
	}
	printf("DevId found!\n");
	devInfo *devInfoEntry = getFromList(&devInfos,devIdIndex);
	unsigned char *pointerToKey = devInfoEntry->key;
	printf("Printing key\n");
	for(int i = 0; i<KEYLEN;i++){
		printf("%c",*(pointerToKey+i));
	}
	printf("\n");

	//Convert add back to network order after getting the key for decryption tag to be true
	pckDataToNetworkOrder(userAddPckData);


	//Decrypt and process
	mbedtls_gcm_context ctx;
	if(initGCM(&ctx,pointerToKey,KEYLEN*8)!=0){
		printf("Failed to init gcm\n");
		exit(1);
	}

	if(decryptPckData(&ctx,processMsgBuffer,decryptedProcessMsgBuffer)==-1){
		printf("Failed decrypting msg\n");
		return -1;
	}
	printf("Data decrypted successfully\n");
	//Clear ctx structure
	mbedtls_gcm_free(&ctx);

	//Get data pointers from decrypted msg
	arrayList decryptedPckDataPointers;
	readDecryptedData(decryptedProcessMsgBuffer,&decryptedPckDataPointers);

	//Execute decrypted message commands based on opCode
	//Get pointers to data in msg
	uint32_t recvNonce = getNonceFromDecryptedData(decryptedProcessMsgBuffer);
	uint32_t pckGSettings = getPckGSettingsFromDecryptedData(decryptedProcessMsgBuffer);
	unsigned char *generalSettingsPtr;
	getElementFromPckData(&decryptedPckDataPointers,&generalSettingsPtr,0);
	unsigned char *opCodePtr;
	getElementFromPckData(&decryptedPckDataPointers,&opCodePtr,1);
	uint32_t argsLen;
	unsigned char *argsPtr;
	argsLen = getElementFromPckData(&decryptedPckDataPointers,&argsPtr,2);


	printf("Nonce recieved = %d\n",recvNonce);

	//Get connInfo for this socket
	connInfo_t *connInfo;
	if((connInfo = findConnInfo(socketBeingProcessed,&connectionsInfo))==NULL){
		printf("No connInfo found. CREATE ONE ON SOCKET CONNECTION. TERMINATING\n");
		exit(1);
	}else{
		//If connInfo exists check if recvNonce is set
		printf("connInfo found. Checking if recvNonce is set\n");
		if(connInfo->isDevIdSet==0){
			printf("Setting DevId in connInfo\n");
			//If devId not set, set it
			memcpy(connInfo->devId,devId,DEVIDLEN);
			connInfo->isDevIdSet = 1;
		}
		if(connInfo->recvNonce==-1){
			//If no recvNonce set yet, set it
			connInfo->recvNonce = recvNonce;
		}else{
			//If recvNonce is set increment it by 1
			connInfo->recvNonce+=1;
		}
	}

	//Get packet's general settings bitfield
	//Get isTerminator (first bit)
	unsigned char isTerminator = (*generalSettingsPtr & (1<<7));
	printf("is terminator: %i\n",isTerminator);
	if(isTerminator>0){
		//Last message recieved from node. Now check if there are queed commands for node if so send them, otherwise close() the socket
		//to let the device go back to sleep
		int findRs = findDevIdInCmdQueue(devId);
		if(findRs==-1){
			printf("Failed to find enqueued commands for this devId, terminating connection\n");
			closeSocket(socketBeingProcessed);
		}else if(findRs==-2){
			printf("Error while finding devId in commandsQueue\n");
			exit(1);
		}else if(findRs>=0){
			//Enqueued command/s found! Send them over
			printf("Found enqueued commands\n");
			processSendQueue(socketBeingProcessed);
		}
	}

	printf("Nonce in server: %d. Nonce recieved: %d\n",connInfo->recvNonce,recvNonce);
	//ONLY EXECUTE COMMAND IF NONCES EQUAL. PREVENTS REPLAY ATTACKS
	if(connInfo->recvNonce==recvNonce){
		//Get opCode
		uint32_t opCode = ntohl(*(uint32_t*)(opCodePtr));
		printf("Op code: %d\n",opCode);
		//Execute command
		uint32_t argsLen = ntohl(*(uint32_t*)argsLenPtr);
		unsigned char isReservedOpcode = (*generalSettingsPtr & 1<<6);
		printf("Is reserved opCode: %d\n",isReservedOpcode);
		executeCommand(devInfoEntry->devType,opCode,isReservedOpcode, argsPtr,argsLen);
	}else{
		//Nonces not equal
		printf("NONCES NOT EQUAL BRUV\n");
	}
}

//If only wanna set recvNonce or sendNonce set the opposite one to -1 (default)
int addConnInfo(int s, uint32_t recvNonce, uint32_t sendNonce){
	connInfo_t connInfo;
	connInfo.recvNonce = recvNonce;
	connInfo.sendNonce = sendNonce;
	connInfo.socket = s;
	connInfo.isDevIdSet = 0;
	addToList(&connectionsInfo,&connInfo);
}

int removeConnInfo(int s){
	for(int i =0; i<connectionsInfo.length;i++){
		connInfo_t *newConnInfo = getFromList(&connectionsInfo,i);
		if(newConnInfo->socket==s){
			printf("Removing conn info!\n");
			removeFromList(&connectionsInfo,i);
			return 0;
		}
	}
}

connInfo_t *findConnInfo(int s, arrayList *connInfos){
	for(int i = 0; i<connInfos->length; i++){
		connInfo_t *connInfo = getFromList(connInfos,i);
		if(connInfo->socket==s){
			return connInfo;
		}
	}
	//If none found return null
	return NULL;
}

//Returns -1 on not found -2 on error and index in commandQueue if found sucessfully
int findDevIdInCmdQueue(unsigned char *devId){
	pthread_mutex_lock(&commandQueueAccessMux);
	for(int i = 0; i<commandsQueue.length; i++){
		commandRequest_t *cmdRequest = getFromList(&commandsQueue,i);
		if(memcmp(devId,cmdRequest->devId,DEVIDLEN)==0){
			//Found commandRequest for this devId
			printf("Found a commandRequest for this devId\n");
			pthread_mutex_unlock(&commandQueueAccessMux);
			return i;
		}
	}
	pthread_mutex_unlock(&commandQueueAccessMux);
	return -1;
}

//Removes connInfo while closing socket. Use instead of close()
/*int closeSocket(int s){
	removeConnInfo(s);
	close(s);
}*/

int processSendQueue(int socket){
	//Find if there is a connInfo (established after connect) and send the enqueue commands
	printf("Processing send queue\n");
	for(int i = 0; i<connectionsInfo.length; i++){
		printf("Looping connInfo\n");
		connInfo_t *connInfo = getFromList(&connectionsInfo,i);
		if(connInfo!=NULL){
			if(connInfo->socket==socket){
				printf("Found corresponding connInfo\n");
				//Found connInfo for this socket
				//Check if there are any commands to be sent if not quit
				pthread_mutex_lock(&commandQueueAccessMux);
				for(int cmdReqIndex =0; cmdReqIndex <commandsQueue.length;cmdReqIndex++){
					printf("Looping commandsQueue\n");
					commandRequest_t *cmdRequest = getFromList(&commandsQueue,cmdReqIndex);
					printf("Comparing dev ids\n");
					for(int i = 0; i <DEVIDLEN; i++){
						printf("%c",*(cmdRequest->devId+i));
					}
					printf("\n");

					for(int i = 0; i <DEVIDLEN; i++){
						printf("%c",*(connInfo->devId+i));
					}
					printf("\n");
					if(memcmp(cmdRequest->devId,connInfo->devId,DEVIDLEN)==0){
						printf("Found enqueued commands for this DevId. Sending them over...\n");
						//Check if sendNonce is set if so +1 it, if not set it
						if(connInfo->sendNonce==-1){
							uint32_t newNonce;
							getrandom(&newNonce,4,0);
							printf("New rand nonce genrated: %d\n",newNonce);
							connInfo->sendNonce = newNonce;
						}else{
							connInfo->sendNonce+=1;
						}
						int numOfCmds = (&(cmdRequest->commandRequestsCommands))->length;
						printf("num of cmds: %d\n",numOfCmds);
						for(int i = 0; i<numOfCmds; i++){
							commandRequestCommand_t *newCmdReq = getFromList(&(cmdRequest->commandRequestsCommands),i);
							//Get the commands from queue and send them over
							int newMsgLen = 2+1+IVLEN+TAGLEN+newCmdReq->addLen+NONCELEN+1+4+4+newCmdReq->argsLen;
							unsigned char newMsgBuf[newMsgLen];
							printf("Generating msg of len: %d\n",2+1+IVLEN+TAGLEN+newCmdReq->addLen+NONCELEN+1+4+4+newCmdReq->argsLen);

							unsigned char gSettings;
							gSettings = newCmdReq->gSettings;
							//Detect if its the last msg in queue
							if((numOfCmds-i)==1){
								//If so, assign terminator msg option in gSettings bitfield
								gSettings |= (1<<7);
							}
							generateCommandsMsg(newMsgBuf,newCmdReq,cmdRequest->devId,connInfo,gSettings);
							if((numOfCmds-i)==1){
								//Last msg sent. Close connection
								printf("Last msg sent\n");
								sendall(connInfo->socket,newMsgBuf,newMsgLen);
								sendall(connInfo->socket,newMsgBuf,newMsgLen);
								closeSocket(connInfo->socket);
							}else{
								sendall(connInfo->socket,newMsgBuf,newMsgLen);
							}

						}
						//Free cmdRequest once finished!
						pthread_mutex_unlock(&commandQueueAccessMux);
						freeCommandRequest(cmdReqIndex);
					}
				}
			}
		}else{
			printf("CONN INFO SHOULDNT BE NULL. TERMINATING\n");
			exit(1);
		}
	}
}

int generateCommandsMsg(unsigned char *newMsgBuf, commandRequestCommand_t *newCmdReq, unsigned char *devId, connInfo_t *connInfo, unsigned char gSettings){
	//No add(additional data (see AES-GCM)) support, yet
	uint16_t msgLen = htons(2+1+IVLEN+TAGLEN+newCmdReq->addLen+NONCELEN+1+4+4+newCmdReq->argsLen);
	printf("New cmd msg len: %d\n",ntohs(msgLen));

	//Get locations in buffer to assign
	unsigned char *msgLenPtr = newMsgBuf;
	unsigned char *addLenPtr = newMsgBuf+2;
	unsigned char *ivPtr = addLenPtr+1;
	unsigned char *tagPtr = ivPtr+IVLEN;
	unsigned char *addPtr;
	if(newCmdReq->addLen==0){
		addPtr = NULL;
	}else{
		addPtr = tagPtr+TAGLEN;
	}
	unsigned char *edataPtr = tagPtr+TAGLEN+newCmdReq->addLen;
	unsigned char *noncePtr = edataPtr;
	unsigned char *gSettingsPtr = noncePtr+NONCELEN;
	unsigned char *opCodePtr = gSettingsPtr+1;
	unsigned char *argsLenPtr = opCodePtr+4;
	unsigned char *argsPtr = argsLenPtr+4;

	//Assign to them
	*(uint16_t*)msgLenPtr = msgLen;
	*addLenPtr = newCmdReq->addLen;
	getrandom(ivPtr,IVLEN,0);
	memcpy(addPtr,newCmdReq->addPtr,newCmdReq->addLen);
	//E.data
	*(uint32_t*)noncePtr = htonl(connInfo->sendNonce);
	*gSettingsPtr = gSettings;
	*(uint32_t*)opCodePtr = htonl(newCmdReq->opCode);
	*(uint32_t*)argsLenPtr = htonl(newCmdReq->argsLen);
	memcpy(argsPtr,newCmdReq->argsPtr,newCmdReq->argsLen);

	//Get key corresponding to this DEVID
	int devIdIndex = -1;
	//printList(&devIdToKey,1);
	for(int i = 0; i<devInfos.length; i++){
		devInfo *tempDevInfo = getFromList(&devInfos,i);
		if(memcmp(tempDevInfo->devId,devId,DEVIDLEN)==0){
			devIdIndex = i;
			break;
		}
		//No such devId found. Stop processing and start password excahnge based on method set in settings.conf. PassExchangeMethod: <0,1,2>
		//0 - preset passwords, drop this connection
		//1 - local area exchange using master exchange password. Decrypt beacon with master pass, send passReq cmd to dev and wait for response, when
		//recieved change encryption passwords to the one randomly generated by node. Done
		//2 - Use TLS for exchange, needs internet access for external server for cetificate verification (local area isnt secure due to ARP poisoning possibility)

		//return -1;
	}
	printf("Dev id index: %d\n",devIdIndex);
	if(devIdIndex==-1){
		printf("No dev info for this device\n");
		return -1;
	}
	printf("DevId found!\n");
	unsigned char *devIdToKeyEntry = getFromList(&devInfos,devIdIndex);
	unsigned char *pointerToKey = devIdToKeyEntry+DEVIDLEN;
	printf("Printing key\n");
	for(int i = 0; i<KEYLEN;i++){
		printf("%c",*(pointerToKey+i));
	}
	printf("\n");

	//Encrypt and auto assign to tag pointer during encryption
	mbedtls_gcm_context ctx;
	if(initGCM(&ctx,pointerToKey,KEYLEN*8)!=0){
		printf("Initing gcm failed\n");
		exit(1);
	}
	int eDataLen = NONCELEN+1+4+4+newCmdReq->argsLen;
	printf("Edata Len: %d\n",eDataLen);
	printf("PLAINTEXT:\n");
	for(int i = 0; i<eDataLen;i++){
		printf("%d ",*(edataPtr+i));
	}

	if(encryptGcm(&ctx,edataPtr,eDataLen,ivPtr,IVLEN,addPtr,newCmdReq->addLen,edataPtr,tagPtr,TAGLEN)!=0){
		printf("Failed to encrypt data!\n");
		exit(1);
	}
	printf("CIPHERTEXT:\n");
	for(int i = 0; i<eDataLen;i++){
		printf("%d ",*(edataPtr+i));
	}
	printf("\n");
	printf("TAG GENERATED:\n");
	for(int i = 0; i<TAGLEN;i++){
		printf("%d ",*(tagPtr+i));
	}
	printf("\n");

	//Print resulting msg!
	printf("Printing resulting msg\n");
	for(int i = 0; i<ntohs(msgLen);i++){
		printf("%d ",*(newMsgBuf+i));
	}
	printf("\n");

}

//Free it cause every commandRequest contains a dynamically allocated arrayList of commands inside
int freeCommandRequest(int index){
	pthread_mutex_lock(&commandQueueAccessMux);
	commandRequest_t *req = getFromList(&commandsQueue,index);
	if(req==NULL){
		printf("CommandRequest index incorrect\n");
		pthread_mutex_unlock(&commandQueueAccessMux);
		return 1;
	}
	//Free all malloc'ed pointers for addData and args for commands
	arrayList *cmdReqCommandsPtr = &(req->commandRequestsCommands);
	int numOfCmds = cmdReqCommandsPtr->length;
	for(int i = 0; i < numOfCmds; i++){
		commandRequestCommand_t *cmdReqPtr = getFromList(cmdReqCommandsPtr,i);
		if(cmdReqPtr!=NULL){
			//Free is safe cause these two always get malloc'ed
			free(cmdReqPtr->argsPtr);
			free(cmdReqPtr->addPtr);
		}
	}
	freeArrayList(&(req->commandRequestsCommands));
	removeFromList(&commandsQueue,index);
	pthread_mutex_unlock(&commandQueueAccessMux);
}

int addToCmdQueue(arrayList *cmdReqQueue,
				unsigned char *devId,
				uint32_t opCode,
				unsigned char gSettings,
				unsigned char *argsPtr,
				uint32_t argsLen,
				unsigned char *addPtr,
				unsigned char addLen)
{
	pthread_mutex_lock(&commandQueueAccessMux);
	//Generate newCmdReqCommands
	commandRequestCommand_t newCmdReqCommands;
	newCmdReqCommands.opCode = opCode;
	newCmdReqCommands.gSettings = gSettings;
	newCmdReqCommands.argsPtr = malloc(argsLen);
	memcpy(newCmdReqCommands.argsPtr,argsPtr,argsLen);
	newCmdReqCommands.addPtr = malloc(addLen);
	memcpy(newCmdReqCommands.addPtr,addPtr,addLen);
	newCmdReqCommands.argsLen = argsLen;
	newCmdReqCommands.addLen = addLen;

	//See if there is a cmdReq for this DEVID
	commandRequest_t *reqToAssignTo = NULL;
	for(int i = 0; i<cmdReqQueue->length; i++){
		commandRequest_t *newReq = getFromList(cmdReqQueue,i);
		if(newReq!=NULL){
			if(memcmp(newReq->devId,devId,DEVIDLEN)==0){
				//Already existing cmdReq found!
				reqToAssignTo = newReq;
			}
		}
	}

	//If no cmdReq found, create a new one
	if(reqToAssignTo==NULL){
		commandRequest_t newCmdReq;
		memcpy(newCmdReq.devId,devId,DEVIDLEN);
		if(addToList(cmdReqQueue,&newCmdReq)!=0){
			printf("Failed adding to coammndsQueue\n");
			exit(1);
		}
		reqToAssignTo = getFromList(cmdReqQueue,cmdReqQueue->length-1);
		initList(&(reqToAssignTo->commandRequestsCommands),sizeof(commandRequestCommand_t));
	}

	if(addToList(&(reqToAssignTo->commandRequestsCommands),&newCmdReqCommands)!=0){
		printf("Failed adding cmdReqCommand to commandsRequestCommands list\n");
		exit(1);
	}
	pthread_mutex_unlock(&commandQueueAccessMux);
}

/*int sendall(int s, unsigned char *buf, int len)
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
*/

//Read devInfos.list and so init the devInfos list
int initDevInfos(arrayList *devInfosList){

}

//Returns new devInfo with key as hashed password and other values just copied
devInfo generateNewDevInfo(unsigned char *DevId, unsigned char *password, uint32_t devType){

}

//Add to devInfos arrayList if there isnt a device with this DevId already and append it to the devInfos.list file
int addNewDevInfo(devInfo *newDevInfo){

}

//Remove from devInfos arrayList and remove from devInfos.list
int removeDevInfo(devInfo *devInfoToRemove){

}

//SETTINGS

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
		}else{
			//If not needed option just copy it to temp file
			//printf("no data but copying\n");
			//printf("%s\n",temp);
			fputs(temp,tempSettingsFile);
		}
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

int initializeSettingsData(globalSettingsStruct *globals){
	printf("Initializing settings\n");
	//Initialize defaults, in case not found in settings file
	globals->passExchangeMethod = 1; //Use master password by default

	unsigned char temp[40];
	FILE *settingsFile;
	int currentCharacters;
	unsigned char *settingPtr;
	char isSettingPtrComplete = 0;
	int tempSettingLen;
	uint32_t currentOption;

	if((settingsFile=fopen(settingsFileName,"r"))==NULL){
		printf("Failed opening settings file\n");
		return -1;
	}

	while(fgets(temp,60,settingsFile)!=NULL){
		currentCharacters = strcspn(temp,"\n");
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
		//printf("%s\n",settingPtr);
		if(memcmp(settingPtr,"passExchangeMethod",18)==0){
			//PASS EXCHANGE OPTION
			printf("Setting passExchangeMethod to %d\n",currentOption);
			globals->passExchangeMethod = currentOption;
		}


	}

}



