//THIS THREAD COUNTS TIME (TO DEALLOCATE OLD recvHolders). It counts time at every countInterval.
//Can be used as main loop for other pereodic functions

#include "timingThread.h"
#include "ArrayList/arrayList.h"
#include "server.h"
#include <unistd.h>
#include "util.h"
#include <memory.h>
#include <pckData/pckData.h>

int timingInterval = 2000;  //Milliseconds
int recvHolderTimeout = 5000;  //Milliseconds
int totalTimePassed = 0;  //Milliseconds


void *timingThread(void *args){
    //Main loop
    while(1){
        
        //Remove recvHolder if its lifetime is above recvHolderMutex is available
        pthread_mutex_lock(&recvHolderMutex);
        
        //printf("Recv holders location in thread: %p\n",&recvHolders);
        printf("Recv holders count: %i\n",recvHolders.length);
        //printf("Recv Holders data size: %i\n",recvHolders.dataSize);
        //printf("Recv Holders list increment: %i\n",recvHolders.listIncrement);
        //printf("Recv Holders blocks allocated: %i\n",recvHolders.allocated);
        
        //Remove loop
        for(int i =0; i<recvHolders.length; i++){
            recvHolder *recvHolderPtr;
            recvHolderPtr = getFromList(&recvHolders,i);
            if(recvHolderPtr!=NULL){
                //printf("RecvHolder time lived: %i\n",recvHolderPtr->timeSinceUsed);
                recvHolderPtr->timeSinceUsed+=timingInterval;
                //Remove recvHolder if it timedout
                if(recvHolderPtr->timeSinceUsed>recvHolderTimeout){
                    //printf("Removing recvHolder from removeQueue\n");
                    removeRecvHolder(&recvHolders,i);
                    //Decrement i because removeFromList shifts all values by one to the left and decrements length by one
                    i--;
                }
            }
        }

        fflush(stdout);
        pthread_mutex_unlock(&recvHolderMutex);
        sleep_ms(timingInterval);
    }
}

