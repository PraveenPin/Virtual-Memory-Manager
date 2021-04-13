#include<stdio.h>
#include<inttypes.h>
#include<stdlib.h>
#include<unistd.h>

#include "list.h"

int addToTidQueue(long int tid, TidQueue *tidQueue){
    if(tidQueue->front == 0){
        tidQueue->front = malloc(sizeof(struct ListNode));
        tidQueue->front->tid = tid;
        tidQueue->back = tidQueue->front;
        tidQueue->front->next = 0;
        tidQueue->back->next = 0;
    }
    else{
        tidQueue->back->next = malloc(sizeof(struct ListNode));
        tidQueue->back = tidQueue->back->next;
        tidQueue->back->tid = tid;
        tidQueue->back->next = 0;
    }
    
    return 1;
}

int isThisThreadInWaitingForMutex(long int tid, TidQueue *waitingThreads){
	struct ListNode* node = waitingThreads->front;
	while(node != NULL){
		if (node->tid == tid){
			return 1;
		}
		node = node->next;
	}
	return 0;
}

void emptyTidQueue(TidQueue *waitingThreads){
    struct ListNode *tempNode = waitingThreads->front;
    while(tempNode != NULL){
        struct ListNode* buf = tempNode->next;
        free(tempNode);
        tempNode=buf;
    }
}


void stateOfTidQueue(TidQueue *waitingThreads){
    struct ListNode *tempNode = waitingThreads->front;
    if(tempNode != NULL){
        printf("Waiting thread IDs on mutex: \t");
    }
    while(tempNode != NULL){
        printf("%ld\t",tempNode->tid);
        tempNode = tempNode->next;
    }
    printf("\n");
}
