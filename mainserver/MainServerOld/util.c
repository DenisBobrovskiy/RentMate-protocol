//UTILITY FUNCTIONS AND GLOBALS
#include "util.h"
#include <stdio.h>
#include <stdarg.h>

void sleep_ms(int millis){
    //printf("Sleeping for %i ms\n",millis);
    struct timespec ts;
    ts.tv_sec = millis/1000;
    ts.tv_nsec = (millis%1000) * 1000000;
    nanosleep(&ts, NULL);
}

//Construct custom packet


