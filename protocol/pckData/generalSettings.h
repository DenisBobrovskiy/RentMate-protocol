#ifndef GENERALSETTINGS_HEADER
#define GENERALSETTINGS_HEADER

//PROTOCOL VERSION
#define currentSystemVersion 0

//PORTS
#define NodeConnectionsPort 3333
#define LANConnectionsPort 4444
#define WanControlSocketPort 23875

//Networking
#define maxConcurentConnectionsToNodeSocket 100  //Max concurent connections to main device Control Socket
#define maxConcurentConnectionsToLANSocket 10   //Max concurent connections to command recieing socket on LAN

//Devices and opCodes
#define maxDevTypes 32
#define maxServerCommands 128
#define maxNodeReservedCommands 512   //Set based on how many commands there are
#define maxCodeLockCommands 128

//PROTOCOL SPECIFIC MACROS
#define DEVIDLEN 16
#define IVLEN 12
#define TAGLEN 16
#define NONCELEN 4
#define KEYLEN 16
#define MAXDEVMSGLEN 255  //Max message len of a message coming from a device
#define MAXUSERCMDMAXLEN 255  //Max message len of a controll message from a user to server
#define MAXMSGLEN 255
#define DEVTYPELEN 4
#define OPCODELEN 4

#define baseNodeMessageLen  DEVIDLEN + DEVTYPELEN + OPCODELEN + 32

#endif
