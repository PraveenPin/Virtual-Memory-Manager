#ifndef QUEUE_H_
#define QUEUE_H

#include "my_pthread_t.h"

struct Node{
    TCB *thread;
    struct Node *next;
};

typedef struct{
    struct Node *front;
    struct Node *back;
}Queue;

int addToQueue(TCB* thread, Queue *queue);

int removeFromQueue(Queue *queue, TCB **thread);

int isQueueEmpty(Queue *queue);

void stateOfQueue(Queue *queue);

void deleteAParticularNodeFromQueue(my_pthread_t tid, Queue *queue, TCB **thread);

#endif
