#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <libgen.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h> 
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "common.h"

//TODO:
/*

*/

void handleSIGINT(int signalNumber);
void moveLine(unsigned int* line, int len);
long getTimestamp();

int main(int argc, char *argv[], char *env[]) {

    //parsing input arguments
    if(argc != 4) {
        printf("\033[1;32mTrucker:\033[0m Invalid count of input arguments.\n");
        exit(100);
    }
   
    unsigned int truckCapacity = strtol(argv[1], NULL, 0);
    unsigned int lineItemsCapacity = strtol(argv[2], NULL, 0);
    unsigned int lineWeightCapacity = strtol(argv[3], NULL, 0);

    if(truckCapacity < 1 || lineItemsCapacity < 1 || lineWeightCapacity < 1) {
        printf("\033[1;32mTrucker:\033[0m Invalid input arguments, must be greater than 0.\n");
        exit(100);
    }

    //signal handler init
    struct sigaction actionStruct;
    actionStruct.sa_handler = handleSIGINT;
    sigemptyset(&actionStruct.sa_mask); 
    sigaddset(&actionStruct.sa_mask, SIGINT); 
    actionStruct.sa_flags = 0;
    sigaction(SIGINT, &actionStruct, NULL); 
    
    //create line and counters
    struct ShareMemory lineSM, lineParamSM;

    //create shared memory for line
    lineSM.mem = setUpShareMemory(MEM_LINE, lineItemsCapacity * sizeof(struct Parcel), IPC_CREAT | STANDARD_PERMISSIONS, 1);

    //create shared memory for tracker args
    lineParamSM.mem = setUpShareMemory(MEM_LINE_PARAM, 1 * sizeof(unsigned int), IPC_CREAT | STANDARD_PERMISSIONS, 2);

    //create semaphores
    // semaphore for first free index in line
    lineSM.sem = setUpSemaphore(SEM_LINE, 1, 1);//albo dać domyślną wartość 0
    blockSem(lineSM.sem);
    // count of free weight on line
    lineParamSM.sem = setUpSemaphore(SEM_LINE_PARAM, 1, 2);// albo dać domyślną wartość 0
    blockSem(lineParamSM.sem);

    //save line parameters to shared memory
    setFreeWeightOnLine(lineWeightCapacity, lineParamSM);

    //clearLine
    struct Parcel* lineClear = (struct Parcel*) shmat(lineSM.mem, 0, 0);
    for(int i = 1; i < lineItemsCapacity; i ++) {
        lineClear[i].timestamp = 0;
        lineClear[i].weight = 0;
        lineClear[i].workerId = 0;
    }

    releaseSem(lineSM.sem);
    releaseSem(lineParamSM.sem);

    //set up parameters
    int truckPlacesCount = 0;
    int lineFreeWeightOnLine = lineWeightCapacity;
    struct Parcel* line;
    int endOfLine = lineItemsCapacity - 1;
    int lineLen = lineItemsCapacity;

    //start main loop
    while (1) {
        blockSem(lineSM.sem);
        
        line = (struct Parcel*) shmat(lineSM.mem, 0, 0);
        moveLine((unsigned int *) line, lineLen);
        if(line[endOfLine].weight > 0) {
            truckPlacesCount++;
            lineFreeWeightOnLine -= line[endOfLine].weight;
            setFreeWeightOnLine(lineFreeWeightOnLine, lineParamSM);

            printf("\033[1;32mTrucker:\033[0m Get parcel: weight: %u, workerID: %u, timeDiff: %ld \n",
                    line[endOfLine].weight,
                    line[endOfLine].workerId,
                    getTimestamp() - line[endOfLine].timestamp
                );
        }
        else {
            printf("\033[1;32mTrucker:\033[0m Waiting for parcel.\n");
        }

        releaseSem(lineSM.sem);

        if(truckPlacesCount == truckCapacity) {
            printf("\033[1;32mTrucker:\033[0m Truck is fully - leave factory.\n");
            truckPlacesCount = 0;
            printf("\033[1;32mTrucker:\033[0m New truck arrived to factory.\n");
        }
    }

    return 0;
}

void handleSIGINT(int signalNumber) {
    printf("\033[1;32mTrucker:\033[0m Receive signal SIGINT.\n");
}

void moveLine(unsigned int* line, int len) {
    for(int i = 1; i < len; i ++) {
        line[i] = line[i - 1];
    }
    line[0];
}

long getTimestamp() {
    struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return timestamp.tv_nsec;
}