#ifndef HEADER_COMPILEFLAG_H
#define HEADER_COMPILEFLAG_H

#define targetPlatform 1 //1 for linux computer, 2 for esp32
#define localPlatform 1 //1 if client is on same device as the server(for testing purposes), 0 if they are separate devices

// 0  - NONE (Dont use)
// 1 - TestDevice (Testing purposes only)
// 2 - SmartLock
#define targetDevtype 2 

#endif
