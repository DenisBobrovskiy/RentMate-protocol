# USE GUIDE:

### Hierarchy:

*  **/protocol** - defines the core code for the transmission protocol such as AES-GCM encryption procedures, how messages are structured, types of messages, how they are sent and recieved and other low level core code used by the rest of the system
* **/misc** - contains helper libraries and their source code  
  * **/AES-GCM** - helper library for AES-GCM encryption, wrapper functions of mbedtls functions for ease of use, contains source and libaesgcm.a library (this library includes all mbedtls libmbedcrypto.a object files, **NOT FIXED**)
  * **/ArrayList** - helper library that introduces a dynamic array that automatically resizes based on how many elements are assigned to it
  * **/mbedtls** - a copy of mbedtls library
* **/mainserver** - code for the master device in the network (Raspberry PI or other linux SoC), multithreaded application that managers every node in a local network and recieves commands from the user be it on local network or from the internet
* **/testing** - code used in testing/debugging the system


### Protocol Basics:

* #### Message Structure  
    NOTE: * - means there is an explanation of this field below  
    NOTE: ** - means there is a more detailed structure table of this field later  <br><br>
    **General message structure**:  

    | name                   | size(bytes)   | limits(bytes)    |
    |------------------------|:-------------:|:------------:     |
    | Length(L1)             | 4             | 0-3               |
    | ADDLength(L2)          | 4             | 4-7               |
    | ExtraDataLen(L3)       | 4             | 8-11              |
    | IV                     | 12            | 12-23             |
    | Tag                    | 16            | 24-39             |
    | ExtraData**            | L3            | 40-(39+L3)        |
    | ADDData**              | L2            | (40+L3)-(39+L3+L2)|
    | EncryptedData**        | L1            | (40+L3+L2)-(L1-1) | 
    
    **PckData structure**(used to store any data that needs to be sent, functions like an array):

    | name                   | size(bytes)   | limits(bytes)|
    | ---                    | :---:         | :---:        |
    | pckDataLen             | 4             | 0-3          |
    | pckDataIncrements*     | 4             | 4-7          |
    | firstElementLength     | 4             | 8-11         |
    | firstElement           | firstElemLen  | 12-...       |
    | secondElementLength    | 4             | ...-...      |
    | secondElement          | secElemLen    | ...-...      |
    
    * NOTE: There can be any number of element like in an array  
    * pckDataIncrements - Used only locally for reallocating more space for new elements  <br><br>
    
    **EncryptedData(From General Structure section) structure**:

    | name                   | size(bytes)   | limits(bytes)|
    | ---                    | :---:         | :---:         |
    | nonce*                 | 4             | 0-3           |
    | pckGSettings*          | 4             | 4-7           |
    | PckData*               | ...           | 8-...         |
    * nonce - sessionID. A random number necessary to avoid msg replay attacks. Obtained by each side sending a random number and then adding them to obtain the sessionID used to communicate. Add 1 to the number on both sides every time a new message is sent. After connection is closed a new sessionID will need to be generated to communicate.
    * pckGSettings - binary field for settings (which ones???)
    * PckData - a PckData structure for storing the main message  <br><br>

    **ADDData obligatory fields**:

    | name                   | size(bytes)   | limits(bytes)|
    | ---                    | :---:         | :---:        |
    | versionNum*            | 4             | 0-3          |
    * versionNum - used to accomodate for older firmware   <br><br>
    
    **ExtraData**:  
    No predefined structure, just put a pckData into it.

* ### How server works 

    #### Startup
    1) Call initBasicServerData(...) on startup, this initializes the server-side stuff including its settings
    2) Boot settings - contained in file settings.conf in same directory as source code. They are read into globalSettingsStruct that you pass to initBasicServerData()
    3) 
    #### Sending/Recieving Data
    There are multiple sockets listening on server at any time. One for sending and recieving node commands. One for recieving and replying to client commands on LAN. One for recieving and replying to client commands from the internet.  
    1) The node will constantly be sending out beacon packets so the server can see them. When a beacon packet is caught from a new device, the server queues it for verification (by client).Then saves the device type and other metadata from the beacon packet. Once verified, a password exchange happens.
    2) When password is established, encrypted messages can be exchanged. Every device has an ID, that is sent as ADD(check AES-GCM encryption protocol). Using this DEVID(unique) the server figures out which key to use by checking the nodeData array, to decrypt the rest of the message.

    #### Storing node data
    Every node has metadata associated with it. This metadata is exchanged upon initial connection. Metadata is stored in the array of structs struct nodeData(contains node's encryption password,its DEVID, its type, etc.), whenever a new nodeData is added or server is stopped, this array get saved into a text file nodeData.txt


* ### How nodes work

    #### Persistent Storage
    Every node has its own config file in "setting":"value" format for every line  
    The **settings** stored in it are:
    | settingName                 | description                                   |
    | ---                         | ---                                           |
    | devId                       | one time randomly generated 128 bit number    |
    | devType                     | type of device, 16 bit number                 |
    |

    #### Startup

    #### Sending/Recieving Data  
    When sending a packet from node to server it is encrypted using a key specific to that node which is established on first connection. Then the server uses DEVID unique to each node sent in plaintext with each packet to figure out which key to use to decrypt the rest of the message.  
    **(From)Node Packet structure:**  

    | name          | size(bytes)        | limits(bytes)      | type      |
    | ---           | :---:              | :---:              | :---:     |
    | DEVID         | 16                 | 0-15               | ADD       |
    | devtype       | 4                  | 0-3                | Encrypted | 
    | opcode        | 4                  | 4-7                | Encrypted |
    | argsLen       | 4                  | 8-11               | Encrypted |
    | args          | argsLen            | 12-(12+argsLen)    | Encrypted |


    #### Op Codes

    Every device(devId) has a different set of opCodes. Also for every device there are 2 sets of opCodes. One for the server command to device and one for the node command to the server  
    
    **devType: 1 (TestDevice):**  
    | opcode    | nodeToServer              | serverToNode              |
    | ---       | ---                       | ---                       |
    | 0         | beacon packet             | poweroff                  |
    | 1         | message packet            | message to node           |



* ### Packet handling

    To build a packet you need 3 pckData structures (Here the data is stored):
    1) addData(verifiable yet non-encrypted data)  
    1) msgData(encrypted and verifiable data)  
    1) extraData(visible non verifiable data(NOTE:Dont use for any important data))  

    You will also need ascilliary fields:
    1) nonce(sessionID) **???WIP**
    1) pckGSettings byte for packet settings
    1) AES-GCM context for encrypting the message **???WIP INTRODUCE SYSTEM TO HANDLE THIS**  

    Once you set up this data:  
    1)Encrypt the message with encryptPckData from pckData.c. This function will encrypt the message with AES-GCM and convert all protocol specific fields to network order. NOTE: Any data you send inside your pckData structures NEEDS to be converted to network order and then back to host order on reception  
    2)
* ### Commands
    **Sent to initiate a certain command, based on an opCode, different opCode lists exist based on devType integer, devType = 0 is server**

    * #### Node to server
    * #### Server to node
    * #### Client to server
    * #### Client to node

* ### Boot settings
    Those are stored in settings.conf and loaded on server/client startup in a plaintext file in format of <key>:<value> one pair per line  
    **Possible options:**  
    1) passExchangeMethod. Values: 0 - standard, 1 - ...

    
