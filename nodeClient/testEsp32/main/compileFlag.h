#ifndef HEADER_COMPILEFLAG_H
#define HEADER_COMPILEFLAG_H

#define targetPlatform 2 //1 for linux computer, 2 for esp32
#define localPlatform 2 //1 if client is on same device as the server(for testing purposes), 0 if they are separate devices

// 0  - NONE (Dont use)
// 1 - TestDevice (Testing purposes only)
// 2 - SmartLock
// 3 - Smart Socket
// 4 - Presence Detector
// 5 - Noise Monitor
#define targetDevtype 5

#endif
