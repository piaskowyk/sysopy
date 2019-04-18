#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include <sys/msg.h> 
#include <sys/ipc.h>
#include <errno.h>

#include "server_const.h"

struct StringArray{
    unsigned int size;
    char** data;
};

int nextClientID = 0;
int clientsQueueId[MAX_CLIENTS_COUNT];

int groupsSize[MAX_CLIENTS_COUNT];
int friendsGroups[MAX_CLIENTS_COUNT][MAX_GROUP_SIZE];

int actualUserId = 0;

void executeCommand(struct message* input, struct message* output);
int userExist(int userId);

void stopCMD(struct message* input, struct message* output);
void listCMD(struct message* input, struct message* output);
void friendsCMD(struct message* input, struct message* output);
void addCMD(struct message* input, struct message* output);
void dellCMD(struct message* input, struct message* output);
void initCMD(struct message* input, struct message* output);
void echoCMD(struct message* input, struct message* output);
void _2allCMD(struct message* input, struct message* output);
void _2friendsCMD(struct message* input, struct message* output);
void _2oneCMD(struct message* input, struct message* output);

int main(int argc, char *argv[], char *env[]) {
    // initialisation server
    char *homedir = getenv("HOME");

    for(int i = 0; i < MAX_CLIENTS_COUNT; i++){
        clientsQueueId[i] = -1;
        groupsSize[i] = -1;
    }

    key_t msg_queue_key;
    int qid;
    struct message message, response;
    if ((msg_queue_key = ftok(homedir, PROJECT_ID)) == (key_t) -1) {
        fprintf(stderr, "\033[1;32mServer:\033[0m Error while getting unique queue key.\n");
        exit(100);
    }

    if ((qid = msgget(msg_queue_key, IPC_CREAT | QUEUE_PERMISSIONS)) == -1) {
        fprintf(stderr, "\033[1;32mServer:\033[0m Error while creating queue.\n");
        exit(101);
    }

    // start listening on user requests
    printf("\033[1;32mServer:\033[0m Server is running\n");

    while (1) {
        // read an incoming message, with priority order
        if (msgrcv(qid, &message, sizeof(struct message_text), -100, 0) == -1) {
            fprintf(stderr, "\033[1;32mServer:\033[0m Error while reading input data.\n");
        } else {
            printf("\033[1;32mServer:\033[0m message received:\n\ttype: %s, id: %d, message: %s \n", 
                typeToStr(message.message_type), 
                message.message_text.id,
                message.message_text.buf
            );
        }

        executeCommand(&message, &response);

        // send reply message to client
        if (
            userExist(actualUserId) && 
            msgsnd(clientsQueueId[actualUserId], &response, sizeof(struct message_text), 0) == -1
        ) {
            fprintf(stderr, "\033[1;32mServer:\033[0m Error while sending data, errno = %d\n", errno);
        }
        else {
            printf("\033[1;32mServer:\033[0m send response to client - %d.\n", 
                    actualUserId
                );
        }
        
    }

    return 0;
}

void executeCommand(struct message* input, struct message* output) {

    actualUserId = input->message_text.id - SHIFTID;

    if(!userExist(actualUserId) && input->message_type != INIT){
        sprintf(output->message_text.buf, "User not exist.");
        output->message_text.id = 0;
        output->message_type = 500;
        return;
    }

    switch (input->message_type) {
        case STOP: {
            stopCMD(input, output);
        } break;
        case LIST: {
            listCMD(input, output);
        } break;
        case FRIENDS: {
            friendsCMD(input, output);
        } break;
        case INIT: {
            initCMD(input, output);
        } break;
        case ECHO: {
            echoCMD(input, output);
        } break;
        case _2ALL: {
            _2allCMD(input, output);
        } break;
        case _2FRIENDS: {
            _2friendsCMD(input, output);
        } break;
        case _2ONE: {
            _2oneCMD(input, output);
        } break;

        default:
            break;
    }

    output->message_text.id = actualUserId + SHIFTID;
    output->message_type = actualUserId + SHIFTID;
    
}

int userExist(int userId) {
    if(userId > nextClientID) return 0;
    return clientsQueueId[userId] != -1;
}

int userQueueExist(int userQueueId) {
    for(int i = 0; i < nextClientID; i++) {
        if(clientsQueueId[i] == userQueueId) return 1;
    }
    return 0;
}

struct StringArray explode(char* string, long len, char delimer) {
    struct StringArray itemsArray;
    char** items = NULL;
    int itemsCount = 0;

    itemsArray.size = 0;
    itemsArray.data = NULL;

    if(len == 0 || string == NULL) return itemsArray;

    itemsCount++;
    for(long i = 0; i < len; i++){
        if(string[i] == delimer) {
            itemsCount++;
        }
    }

    items = calloc(itemsCount, sizeof(char*));

    int indexGlob, indexStart;
    indexGlob = indexStart = 0;
    for(int i = 0; i < itemsCount; i++) {
        indexStart = indexGlob;
        while(indexGlob < len && string[indexGlob] != delimer) indexGlob++;

        if(indexGlob == indexStart){
            itemsCount--;
            i--;
            continue;
        }
        items[i] = calloc(indexGlob - indexStart + 1, sizeof(char));
        memcpy(items[i], string + indexStart, (indexGlob - indexStart) * sizeof(char));
        indexGlob++;
    }
    
    itemsArray.size = itemsCount;
    itemsArray.data = items;

    return itemsArray;
}

void prepareMessage(struct message* input, struct message* output) {
    __time_t now;
    time(&now);
    char date[21];
    strftime(date, 21, "%d-%m-%Y_%H:%M:%S", localtime(&now));
    sprintf(output->message_text.buf, "from %d, %s - %s", actualUserId, input->message_text.buf, date);
}

int sendMessage(int id, struct message* output) {
    if (userExist(id) && msgsnd(clientsQueueId[id], &output, sizeof(struct message_text), 0) == -1) {
        fprintf(stderr, "\033[1;32mServer:\033[0m Error while sending data, errno = %d\n", errno);
        return -1;
    }
    else {
        printf("\033[1;32mServer:\033[0m send response to client %d from %d.\n", id, actualUserId);
        return 1;
    }
}

int getFreeIndex() {
    if(nextClientID < MAX_CLIENTS_COUNT){
        nextClientID++;
        return nextClientID - 1;
    }
    else {
        for(int i = 0; i < MAX_CLIENTS_COUNT; i++){
            if(clientsQueueId[i] == -1) return i;
        }
    }
}

int existInGroup(int actualUserId, int friendsId) {
    for(int i = 0; i < groupsSize[actualUserId]; i++) {
        if(friendsGroups[actualUserId][i] == friendsId) return 1;
    }
    return 0;
}

//------------------------------------------------------------------------------------------

void stopCMD(struct message* input, struct message* output) {
    //remove queue
    clientsQueueId[actualUserId] = -1;
    
    // remove from any group
    for(int i = 0; i < nextClientID; i++) {
        for(int j = 0; j < groupsSize[i]; j++) {
            if(friendsGroups[i][j] == actualUserId) {
                for(int k = j; k < groupIndex - 1; k++) {
                    friendsGroups[i][k] = friendsGroups[i][k + 1];
                }
                groupsSize[i]--;
                break;
            }
        }
    }

    sprintf(output->message_text.buf, "User is removed.");
}

void listCMD(struct message* input, struct message* output) {
    char item[10];
    int offset = 0;
    for(int i = 0; i < nextClientID; i++){
        if(i != nextClientID - 1) sprintf(item, "%d, ", i);
        else sprintf(item, "%d", i);
        memcpy(output->message_text.buf + offset, item, strlen(item) * sizeof(char));
        offset += strlen(item);
    }
    *(output->message_text.buf + offset) = '\0';
}

void friendsCMD(struct message* input, struct message* output) {
    //explode ids list
    struct StringArray idList = explode(input->message_text.buf, strlen(input->message_text.buf), ',');
    
    if(idList.size > MAX_GROUP_SIZE){
        sprintf(output->message_text.buf, "To many users.");
        return;
    }

    int groupSize = 0;;
    for(int i = 0; i < idList.size; i++) {
        friendsGroups[actualUserId][groupSize] = strtol(idList.data[i], NULL, 0);
        if(
            friendsGroups[actualUserId][groupSize] >= 0 && 
            userExist(friendsGroups[actualUserId][groupSize]) &&
            !existInGroup(actualUserId, friendsGroups[actualUserId][groupSize])
        ) {
            groupSize++;
        }
    }
    groupsSize[actualUserId] = groupSize;

    if(groupSize == 0) {
        sprintf(output->message_text.buf, "Empty group.");
    }
    else {
        sprintf(output->message_text.buf, "Create group with size %d.", groupsSize[actualUserId]);
    }
}

void addCMD(struct message* input, struct message* output) {
    //explode ids list
    struct StringArray idList = explode(input->message_text.buf, strlen(input->message_text.buf), ',');
    
    int groupIndex = groupsSize[actualUserId];
    if(idList.size + groupIndex > MAX_GROUP_SIZE){
        sprintf(output->message_text.buf, "To many users.");
        return;
    }

    for(int i = 0; i < idList.size; i++) {
        friendsGroups[actualUserId][groupIndex] = strtol(idList.data[i], NULL, 0);
        if(
            friendsGroups[actualUserId][groupIndex] >= 0 && 
            userExist(friendsGroups[actualUserId][groupIndex]) &&
            !existInGroup(actualUserId, friendsGroups[actualUserId][groupSize])
        ) {
            groupIndex++;
        }
    }
    groupsSize[actualUserId] = groupIndex;

    if(groupIndex == 0) {
        sprintf(output->message_text.buf, "Empty group.");
    }
    else {
        sprintf(output->message_text.buf, "Create group with size %d.", groupsSize[actualUserId]);
    }
}

void dellCMD(struct message* input, struct message* output) {
    int userId = actualUserId;
    //explode ids list
    struct StringArray idList = explode(input->message_text.buf, strlen(input->message_text.buf), ',');

    //remove grom group
    int userGroupId = 0;
    int groupIndex = groupsSize[userId];
    for(int i = 0; i < idList.size; i++) {
        userGroupId = strtol(idList.data[i], NULL, 0);
        for(int j = 0; j < groupIndex; j++) {
            if(friendsGroups[userId][j] == userGroupId){
                for(int k = j; k < groupIndex - 1; k++) {
                    friendsGroups[userId][k] = friendsGroups[userId][k + 1];
                }
                groupIndex--;
                break;
            }
        }
    }
    groupsSize[userId] = groupIndex;

    if(groupIndex == 0) {
        sprintf(output->message_text.buf, "Empty group.");
    }
    else {
        sprintf(output->message_text.buf, "New group size is %d.", groupsSize[userId]);
    }
}

void initCMD(struct message* input, struct message* output) {
    int userId = strtol(input->message_text.buf, NULL, 0);
    
    if(userQueueExist(userQueueId)){
        sprintf(output->message_text.buf, "User already exist.");
    }
    else {
        int index = getFreeIndex();
        if(index != -1){
            sprintf(output->message_text.buf, "%d", index);
            clientsQueueId[index] = userId;   
        }
        else {
            sprintf(output->message_text.buf, "Too many clients.");
        }
    }

    actualUserId = index;
    output->message_type = index;
}

void echoCMD(struct message* input, struct message* output) {
    __time_t now;
    time(&now);
    char date[21];
    strftime(date, 21, "%d-%m-%Y_%H:%M:%S", localtime(&now));
    sprintf(output->message_text.buf, "%s - %s", input->message_text.buf, date);
}

void _2allCMD(struct message* input, struct message* output) {
    prepareMessage(input, output);

    int sendCounter = 0;
    for(int i = 0; i < nextClientID; i++) {
        if(i != actualUserId && sendMessage(i, output)) sendCounter++;
    }

    sprintf(output->message_text.buf, "Send %d/%d message", sendCounter, nextClientID-1);
    sendMessage(actualUserId, output);
}

void _2friendsCMD(struct message* input, struct message* output) {
    prepareMessage(input, output);

    int sendCounter = 0;
    for(int i = 0; i < groupsSize[actualUserId]; i++) {
        if(friendsGroups[actualUserId][i] != actualUserId && sendMessage(i, output)) sendCounter++;
    }

    sprintf(output->message_text.buf, "Send %d/%d message", sendCounter, groupsSize[actualUserId]-1);
    sendMessage(actualUserId, output);
}

void _2oneCMD(struct message* input, struct message* output) {
    prepareMessage(input, output);

    if(!userExist(input->message_text.additionalArg)){
        sprintf(output->message_text.buf, "Destination user (%d) not exist", input->message_text.additionalArg);
    }

    sendMessage(input->message_text.additionalArg, output);

    sprintf(output->message_text.buf, "OK, send message");
    sendMessage(actualUserId, output);
}