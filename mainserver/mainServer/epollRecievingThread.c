#include "epollRecievingThread.h"
#include "pckData/generalSettings.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
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


#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define MAXEVENTS 255

void *epollRecievingThread(void *args){
    printf2("epollRecievingThread started\n");
    //Defines 2 sockets to accept connections.
    //One for node connections on port - NodeConnectionsPort(generalSettings.h)
    //One for user connections on port - LANConnectionsPort(generalSettings.h)
    
    char NodeConnectionsPortStr[10];
    char LANConnectionsPortStr[10];
    struct addrinfo hintsNode, hintsLAN, *nodeSocketInfo, *LANSocketInfo, *p1, *p2;
    int err;
    int sockNodeLocal, sockLANLocal, sockNodeRemote, sockLANRemote;
    int yes = 1;
    struct epoll_event ev1, ev2, events[MAXEVENTS];
    int epollMain;  //Epoll socket, handles both NODE and LAN sockets + all connections
    int socketBeingProcessed = -1;  //Used during epoll
    socklen_t sin_size;
	struct sockaddr_storage remote_addr;  //Remote addr info. Used during epoll
	char connAddr[INET_ADDRSTRLEN]; //Used to temporarily store IPs of connections in string form
    unsigned char processMsgBuffer[MAXMSGLEN];


    //Zero hints to avoid errors
    memset(&hintsNode,0,sizeof(hintsNode));
    memset(&hintsLAN,0,sizeof(hintsLAN));

    //Set hints
    hintsNode.ai_family = AF_UNSPEC;
    hintsLAN.ai_family = AF_UNSPEC;
    hintsNode.ai_socktype = SOCK_STREAM;
    hintsLAN.ai_socktype = SOCK_STREAM;
    hintsNode.ai_flags = AI_PASSIVE;
    hintsLAN.ai_flags = AI_PASSIVE;

    
    //Get addr info (simplifies handling a socket)
    sprintf(NodeConnectionsPortStr,"%d",NodeConnectionsPort);
    sprintf(LANConnectionsPortStr,"%d",LANConnectionsPort);
    if((err = getaddrinfo(NULL, NodeConnectionsPortStr, &hintsNode, &nodeSocketInfo)) != 0){
        fprintf(stderr, "getaddrinfo %s\n", gai_strerror(err));
        pthread_exit((void*)1);
    }
    if((err = getaddrinfo(NULL,LANConnectionsPortStr, &hintsLAN, &LANSocketInfo)) != 0){
        fprintf(stderr, "getaddrinfo %s\n", gai_strerror(err));
        pthread_exit((void*)1);
    }

    //NODE SOCKET INITIALIZATION (Nodes(devices) will connect to this socket)
    //
    //Loop through addrinfo results to see if a socket can be set and then set it
    for(p1 = nodeSocketInfo; p1!=NULL; p1->ai_next){
        
        //Create a socket
        if((sockNodeLocal = socket(p1->ai_family, p1->ai_socktype,p1->ai_protocol)) == -1){
            //Error while settings socket
            perror("Error settings Node socket: ");
            pthread_exit((void*)1);
        }

        //Make the sockets reusable (since a removed socket can occupy space for some time hence preventing the program from executing if it was run recently, this instruct to reuse those old sockets)
        if(setsockopt(sockNodeLocal, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            perror("Error during setsockopt REUSEADDR on node socket");
            pthread_exit((void*)1);
        }

        //Bind the socket
        if(bind(sockNodeLocal,p1->ai_addr,p1->ai_addrlen) == -1){
            perror("Error while binding node socket");
            pthread_exit((void*)1);
        }
        break;
    }
    //LAN SOCKET INITIALIZATION (a user on a LAN will connect to this socket to control the system)
    //
    //same stuff as for the node socket
    for(p2 = LANSocketInfo; p2!=NULL; p2->ai_next){
        if((sockLANLocal = socket(p2->ai_family,p2->ai_socktype,p2->ai_protocol)) == -1){
            //Error while settings socket
            perror("Error settings LAN socket");
            pthread_exit((void*)1);
        }
        if(setsockopt(sockLANLocal, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            perror("Error during setsockopt REUSEADDR on LAN socket");
            pthread_exit((void*)1);
        }
        if(bind(sockLANLocal,p2->ai_addr,p2->ai_addrlen)== -1){
            perror("Error while binding LAN socket");
            pthread_exit((void*)1);
        }
        break;
    }

    //Free up addrinfo structs since they get dynamically allocated
    freeaddrinfo(nodeSocketInfo);
    freeaddrinfo(LANSocketInfo);

    //Check if any usabled addresses were actually found
    if(p1==NULL){
        fprintf(stderr,"Failed finding node address");
        pthread_exit((void*)1);
    }
    if(p2==NULL){
        fprintf(stderr,"Failed finding LAN address");
        pthread_exit((void*)1);
    }

    //Start listening on both sockets
    if(listen(sockNodeLocal,maxConcurentConnectionsToNodeSocket) == -1){
        perror("Failed starting to listen on node socket");
        pthread_exit((void*)1);
    }
    if(listen(sockLANLocal,maxConcurentConnectionsToLANSocket) == -1){
        perror("Failed starting to listen on LAN socket");
        pthread_exit((void*)1);
    }

    //Instantiate epoll instance
    epollMain = epoll_create1(0);
    if(epollMain == -1){
        perror("epoll_create1 failed");
        pthread_exit((void*)1);
    }

    //Set up sockets for epoll. Level triggered(EPOLLIN) or Edge triggered(EPOLLET)
    ev1.events = EPOLLIN;    //TEMP. CHANGE TO EDGE TRIGGERED (EPOLLET)
    ev1.data.fd = sockNodeLocal;
    ev2.events = EPOLLIN;    //TEMP. CHANGE TO EDGE TRIGGERED (EPOLLET)
    ev2.data.fd = sockLANLocal;

    //Add sockets to the epoll instance
    if(epoll_ctl(epollMain,EPOLL_CTL_ADD,sockNodeLocal,&ev1) == -1){
        perror("epoll_ctl add on node socket failed");
        pthread_exit((void*)1);
    }
    if(epoll_ctl(epollMain,EPOLL_CTL_ADD,sockLANLocal,&ev2) == -1){
        perror("epoll_ctl add on LAN socket failed");
        pthread_exit((void*)1);
    }

    printf2("Listening for nodes on port: %d\n",NodeConnectionsPort);
    printf2("Listening for LAN user connections on port: %d\n",LANConnectionsPort);

    //Initialization successful
    printf2("Initialization successful\n");
    printf2("Waiting for connections....\n");

    //Main loop to iterate epoll
    while(1){
        int totalEvents = epoll_wait(epollMain, events, MAXEVENTS, -1);
        printf2("Waiting for new connections...\n");

        //If any events occured
        if(totalEvents>0){
            //Loop events
            for(int i = 0; i < totalEvents; i++){
                socketBeingProcessed = events[i].data.fd;
                
                //Check if we got a connection from node socket
                if(events[i].data.fd==sockNodeLocal){
                    //New connection on node socket
                    sin_size = sizeof(remote_addr);
                    sockNodeRemote = accept1(sockNodeLocal, (struct sockaddr*)&remote_addr, &sin_size, &connectionsInfo);
                    if(sockNodeRemote == -1){
                        //Accepting connection failed
                        perror("Accepting node socket failed");
                        pthread_exit((void*)1);
                    }
                    //Get the IP of a new connection
                    inet_ntop(AF_INET, &(((struct sockaddr_in*)&remote_addr)->sin_addr),connAddr,INET_ADDRSTRLEN);
                    printf2("Accepting new connection from a node at %s\n", connAddr);

                    //Set a new connected socket as non-blocking
                    if(fcntl(sockNodeRemote,F_SETFL,O_NONBLOCK) == -1){
                        perror("Failed settings a new node connection socket as non-blocking");
                        pthread_exit((void*)1);
                    }
                    //Set this socket up for epoll
                    ev1.events = EPOLLIN;
                    ev1.data.fd = sockNodeRemote;
                    if(epoll_ctl(epollMain, EPOLL_CTL_ADD, sockNodeRemote, &ev1) == -1){
                        perror("Failed adding a new node connection socket to epoll");
                        pthread_exit((void*)1);
                    }

                    printf2("Sucessfully accepted and configured new connection on node socket\n");

                }
                //Check if we got a connection from LAN (user) socket
                else if(events[i].data.fd == sockLANLocal){
                    sin_size = sizeof(remote_addr);
                    sockLANRemote = accept1(sockLANLocal, (struct sockaddr*)&remote_addr, &sin_size, &connectionsInfo);
                    if(sockLANRemote == -1){
                        //Accepting connections failed
                        perror("Accepting LAN socket failed");
                        pthread_exit((void*)1);
                    }
                    //Get the IP of a new connection
                    inet_ntop(AF_INET,&(((struct sockaddr_in*)&remote_addr)->sin_addr), connAddr, INET_ADDRSTRLEN);
                    printf2("Accepting new connection from user at %s\n", connAddr);

                    //Set a new connected socket as non-blocking
                    if(fcntl(sockLANRemote, F_SETFL, O_NONBLOCK) == -1){
                        perror("Failed settings new LAN connection as non-blocking");
                        pthread_exit((void*)1);
                    }
                    //Set the new connected socket for epoll
                    ev2.events = EPOLLIN;
                    ev2.data.fd = sockLANRemote;
                    if(epoll_ctl(epollMain, EPOLL_CTL_ADD, sockNodeRemote, &ev2) == -1){
                        perror("Failed adding new LAN connection to epoll");
                        pthread_exit((void*)1);
                    }
                    printf2("Successfully accepted and configured new connection on LAN socket\n");
                }
                //If a message was recieved from any of the connected devices (nodes or users)
                else if(events[i].data.fd!=sockNodeLocal && events[i].data.fd!=sockLANLocal && events[i].events == EPOLLIN){
                    printf2("Accepting a new message\n");
                    recvAll(&recvHolders,&connectionsInfo, events[i].data.fd, processMsgBuffer, processMsg);
                    int currentSocket = events[i].data.fd;

                    connInfo_t *currentConnection;
                    //Find the connection info
                    for(int i = 0; i<connectionsInfo.length;i++){
                        currentConnection = getFromList(&connectionsInfo,i);
                        if(currentConnection->socket == currentSocket){
                            //Found the connection info!
                            int stage = currentConnection->sessionIdState;
                            printf2("Recieving msg. Connection info state: %d\n", stage);
                            break;
                        }
                        //No connection info(Should have been generated on connection). Terminate this connection.

                    }
                }
            }
        }


    }
}

//Custom printf. Prepends a message with a prefix to simplify analysing output
static int printf2(char *formattedInput, ...){
    int result;
    va_list args;
    va_start(args,formattedInput);
    printf(ANSI_COLOR_YELLOW "epollRecievingThread: " ANSI_COLOR_RESET);
    result = vprintf(formattedInput,args);
    va_end(args);
    return result;
}

