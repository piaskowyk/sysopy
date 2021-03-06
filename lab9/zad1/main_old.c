#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "utils.h"

int passengerCount;
int carriageCount;
int carriageCapacity;
int tourCount;

int* passengerQueue;
int** seatsInCarriage;
int* carriageSeatsState;
int actualLoadCarriage = -1;
int actualReleaseCarriage = -1;

int* passengerState;
int waitForEntry;
int waitForRelease;
int waitForStart;
int startClicker;
int stoppedCarriageCounter = 0;
int queueFreeIndex = 0;

void *threadCarriage(void *data);
void *threadPassenger(void *data);

void addPassengerToQueue(int id);
int getFromPassengerQueue();

void setPassengerState(int id, int state);

void notifyAllAboutStoppedCarriage();

pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t passengerStateMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t loadCarriageMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t releaseCarriageMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t* passengerChangeStateCond;
pthread_mutex_t stoppedCarriageCounterMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waitForStartMutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t carriageLoadMeCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t carriageReleaseCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t waitForEntryCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t waitForReleaseCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t waitForStartCond = PTHREAD_COND_INITIALIZER;

int main(int argc, char *argv[], char *env[]) {

    if(argc < 5){
        printErrorMessage("Not enough arguments", 1);
    }

    passengerCount = strtol(argv[1], NULL, 0);
    if(passengerCount < 1) {
        printErrorMessage("Passenger count must by greater than 0", 1);
    }

    carriageCount = strtol(argv[2], NULL, 0);
    if(carriageCount < 1) {
        printErrorMessage("Carriage count must by greater than 0", 1);
    }

    carriageCapacity = strtol(argv[3], NULL, 0);
    if(carriageCapacity < 1) {
        printErrorMessage("Carriage capacity must by greater than 0", 1);
    }

    tourCount = strtol(argv[4], NULL, 0);
    if(tourCount < 1) {
        printErrorMessage("Tour count must by greater than 0", 1);
    }

    if(passengerCount < carriageCount * carriageCapacity) {
        printErrorMessage("Too enough passenger", 1);
    }

    //array init
    carriageSeatsState = calloc(carriageCount, sizeof(int));
    seatsInCarriage = calloc(carriageCount, sizeof(int));
    for(int i = 0; i < carriageCount; i++) {
        seatsInCarriage[i] = calloc(carriageCapacity, sizeof(int));
    }
    passengerQueue = calloc(passengerCount, sizeof(int));
    passengerState = calloc(passengerCount, sizeof(int));

    passengerChangeStateCond = calloc(passengerCount, sizeof(pthread_cond_t));
    for(int i = 0; i < passengerCount; i++) {
        passengerChangeStateCond[i] = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    }

    //creating carriage threads
    pthread_t* carriageKey = calloc(carriageCount, sizeof(pthread_t));
    int* carriageID = calloc(carriageCount, sizeof(int));

    for(int i = 0; i < carriageCount; i++) {
        carriageID[i] = i + 1;
        if(pthread_create(&carriageKey[i], NULL, threadCarriage, &carriageID[i]) != 0) {
            printErrorMessage("Unable to create thread", 2);
        }
    }

    //creating passenger threads
    pthread_t* passengerKey = calloc(passengerCount, sizeof(pthread_t));
    int* passengerID = calloc(passengerCount, sizeof(int));

    for(int i = 0; i < passengerCount; i++) {
        passengerID[i] = i + 1;
        addPassengerToQueue(passengerID[i]);
        if(pthread_create(&passengerKey[i], NULL, threadPassenger, &passengerID[i]) != 0) {
            printErrorMessage("Unable to create thread", 3);
        }
    }

    actualLoadCarriage = 1;
    actualReleaseCarriage = 1;
    pthread_cond_broadcast(&carriageLoadMeCond);

    //ending carriage threads
    for(int i = 0; i < carriageCount; i ++) {
        pthread_join(carriageKey[i], NULL);
        printf("\033[1;33m>:\033[0m Carriage's thread is end - %d, time: %ld\n",
            i,
            getTimestamp()
        );
    }

    //ending passenger threads
    for(int i = 0; i < passengerCount; i ++) {
        pthread_join(passengerKey[i], NULL);
        printf("\033[1;32m>:\033[0m Passenger's thread is end - %d, time: %ld\n",
               i,
               getTimestamp()
        );
    }

    free(carriageKey);
    free(carriageID);

    free(passengerKey);
    free(passengerID);

    pthread_mutex_destroy(&queueMutex);
    //zrobić destroy dla wszystkich i potem free na tablicy
    //pthread_condattr_destroy

    printf("\033[1;34m>:\033[0m END\n");

    return 0;
}

void *threadCarriage(void *data) {
    int id = *((int*)data);
    int index = id - 1;

//    printf("\033[1;33m[%ld]>:\033[0m Create new carriage (%d)\n",
//            getTimestamp(),
//           id);

    for(int tour = 0; tour < tourCount; tour++) {

        pthread_mutex_lock(&loadCarriageMutex);
        while (actualLoadCarriage != id) {
printf("released_carriage %d,%d\n", id, actualLoadCarriage);
            pthread_cond_wait(&carriageLoadMeCond, &loadCarriageMutex);
        }

        printf("\033[1;33m[%ld]>:\033[0m Carriage %d open door.\n",
               getTimestamp(),
               id);




        startClicker = -1;
        //load passengers
        while (carriageSeatsState[index] < carriageCapacity) {
            int passengerId = getFromPassengerQueue();
            seatsInCarriage[index][carriageSeatsState[index]] = passengerId;
            carriageSeatsState[index]++;

            setPassengerState(passengerId, 1);
            waitForEntry = 1;
            pthread_cond_broadcast(&passengerChangeStateCond[passengerId - 1]);

            while (waitForEntry) {
                pthread_cond_wait(&waitForEntryCond, &loadCarriageMutex);
            }
        }

        printf("\033[1;33m[%ld]>:\033[0m Carriage %d close door.\n",
               getTimestamp(),
               id);




        pthread_mutex_lock(&waitForStartMutex);
        startClicker = seatsInCarriage[actualLoadCarriage - 1][rand() % carriageCapacity];
        waitForStart = 1;
        pthread_mutex_unlock(&waitForStartMutex);

        pthread_cond_broadcast(&waitForStartCond);
        while (waitForStart) {
            pthread_cond_wait(&waitForStartCond, &loadCarriageMutex);
        }

        printf("\033[1;33m[%ld]>:\033[0m Carriage %d start ride.\n",
               getTimestamp(),
               id);


        if(actualLoadCarriage == carriageCount){
            actualLoadCarriage = 1;
        }
        else {
            actualLoadCarriage++;
        }
        pthread_mutex_unlock(&loadCarriageMutex);
        pthread_cond_broadcast(&carriageLoadMeCond);




        //release passenger
        pthread_mutex_lock(&releaseCarriageMutex);
        while (actualReleaseCarriage != id) {
            pthread_cond_wait(&carriageReleaseCond, &releaseCarriageMutex);
        }

        for (int i = 0; i < carriageCapacity; i++) {
            int passengerId = seatsInCarriage[index][--carriageSeatsState[index]];
            setPassengerState(passengerId, 0);
            waitForRelease = 1;
            pthread_cond_broadcast(&passengerChangeStateCond[passengerId - 1]);
            while (waitForRelease) {
                pthread_cond_wait(&waitForReleaseCond, &loadCarriageMutex);
            }
        }

        if(actualReleaseCarriage == carriageCount){
            actualReleaseCarriage = 1;
        }
        else {
            actualReleaseCarriage++;
        }
        pthread_mutex_unlock(&releaseCarriageMutex);



    }

    pthread_mutex_lock(&stoppedCarriageCounterMutex);
        stoppedCarriageCounter++;
    pthread_mutex_unlock(&stoppedCarriageCounterMutex);
//    pthread_cond_broadcast(&stoppedCarriageCounterCond);
    notifyAllAboutStoppedCarriage();

    return NULL;
}

void *threadPassenger(void *data) {
    int id = *((int*)data);
    int index = id - 1;
//    printf("\033[1;32m[%ld]>:\033[0m Create new passenger (%d) thread.\n", getTimestamp(), id);


    while(stoppedCarriageCounter < carriageCount) {



        //wait for entry to carriage
        pthread_mutex_lock(&passengerStateMutex);
        while (passengerState[id - 1] != 1) {
            if(stoppedCarriageCounter == carriageCount) {
                return NULL;
            }
            pthread_cond_wait(&passengerChangeStateCond[id - 1], &passengerStateMutex);
        }

        printf("\033[1;32m[%ld]>:\033[0m Passenger %d entry to carriage (%d) %d/%d.\n",
               getTimestamp(),
               id,
               actualLoadCarriage,
               carriageSeatsState[actualLoadCarriage - 1],
               carriageCapacity);

        pthread_mutex_unlock(&passengerStateMutex);
        waitForEntry = 0;
        pthread_cond_broadcast(&waitForEntryCond);




        //someone must press big red button with label start
        pthread_mutex_lock(&waitForStartMutex);
        while (startClicker == -1) {
            pthread_cond_wait(&waitForStartCond, &waitForStartMutex);
        }
        pthread_mutex_unlock(&waitForStartMutex);

        if (startClicker == id) {
            waitForStart = 0;
            printf("\033[1;32m[%ld]>:\033[0m Passenger %d press button start.\n",
                   getTimestamp(),
                   id);
            pthread_cond_broadcast(&waitForStartCond);
        }




        //wait for release carriage
        pthread_mutex_lock(&passengerStateMutex);
        while (passengerState[index] != 0) {
            pthread_cond_wait(&passengerChangeStateCond[index], &passengerStateMutex);
        }
        addPassengerToQueue(id);
        printf("\033[1;32m[%ld]>:\033[0m Passenger %d released carriage (%d) %d/%d.\n",
               getTimestamp(),
               id,
               actualReleaseCarriage,
               carriageSeatsState[actualReleaseCarriage - 1],
               carriageCapacity);

        pthread_mutex_unlock(&passengerStateMutex);
            waitForRelease = 0;
            pthread_cond_broadcast(&waitForReleaseCond);
        pthread_mutex_unlock(&passengerStateMutex);
//printf("mleko_passenger %d,%d,%d,%d\n",id, passengerState[index],startClicker, actualLoadCarriage - 1);



    }

    return NULL;
}

void addPassengerToQueue(int id) {
    pthread_mutex_lock(&queueMutex);
    if(queueFreeIndex < passengerCount) {
        passengerQueue[queueFreeIndex++] = id;
    }
    else {
        printErrorMessage("Queue is fully", 10);
    }
    pthread_mutex_unlock(&queueMutex);
}

int getFromPassengerQueue() {
    pthread_mutex_lock(&queueMutex);
    int passengerId = 0;

    if(passengerQueue[0] == 0) {
        printErrorMessage("Queue is Empty", 10);
    }
    else {
        passengerId = passengerQueue[0];
        for(int i = 1; i < queueFreeIndex; i++) {
            passengerQueue[i - 1] = passengerQueue[i];
        }
        queueFreeIndex--;
    }
    pthread_mutex_unlock(&queueMutex);
    return passengerId;
}

void setPassengerState(int id, int state) {
    passengerState[id - 1] = state;
}

void notifyAllAboutStoppedCarriage() {
    for(int i = 0; i < passengerCount; i++) {
        pthread_cond_broadcast(&passengerChangeStateCond[i]);
    }
}