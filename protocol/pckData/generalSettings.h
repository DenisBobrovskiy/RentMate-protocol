#ifndef GENERALSETTINGS_HEADER
#define GENERALSETTINGS_HEADER

//PROTOCOL VERSION
#define currentSystemVersion 0

//WAKEUP INTERVAL
#define wakeUpIntervalMs 3000 //Wake up interval in milliseconds

//PORTS
#define NodeConnectionsPort 3333
#define LANConnectionsPort 4444
#define WanControlSocketPort 23875
#define NodeRecievePort 5555  //Port for recieving messages from the server on the node

//Networking
#define maxConcurentConnectionsToNodeSocket 100  //Max concurent connections to main device Control Socket
#define maxConcurentConnectionsToLANSocket 10   //Max concurent connections to command recieing socket on LAN
#define maxConcurentConnectionsToNode 5  //Should be 1 but for testing purposes leave at 5

//Devices and opCodes
#define maxDevTypes 32
#define maxServerCommands 128
#define maxNodeReservedCommands 512   //Set based on how many commands there are
#define maxCodeLockCommands 128
#define maxTestDeviceCommands 255
#define nodeToServerReservedOpCodesUpperLimit 128 //This means all opcodes from 0-128 are reserved for device agnostic nodeToServer commands (like beacon command for instance which just displays a message from node on the server console)

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
