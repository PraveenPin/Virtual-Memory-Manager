// File:	my_pthread_t.h
// Date:	3/19/2021
// Author: Omkar Dubas, Praveen Pinjala, Shika Vyaghra
// username of iLab:
// iLab Server: 

#ifndef _UTHREADLIB_H_
#define _UTHREADLIB_H_

#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/time.h>
#include <time.h>
#include "list.h"

#define STACK_SIZE (1024*1024)
#define NUMBER_OF_LEVELS 3
#define MAX_THREADS 128

typedef enum{
	READY,
	RUNNING,
	WAITING,
	FINISHED
}my_pthread_state;

typedef long int my_pthread_t;

typedef struct threadControlBlock {
	char my_pthread_name[31]; //name for a thread if required
	void *stack;
	ucontext_t *context; // context for this thread
	my_pthread_t id; // thread id
	my_pthread_state state; //thread state
	void **retVal; //return value from the function
	my_pthread_t waiting_id; //Thread id of the thread waiting on this thread

    my_pthread_t mutex_acquired_thread_id;
    int priority; // thread priority
	double timeSpentInSeconds,timeSpentInMilliSeconds,totalTimeInSecs,totalTimeInMilliSecs;
	struct timespec created, start,resume, finish;
	int firstCycle;

	int hasMutex;
} TCB; 

/* mutex struct definition */
typedef struct my_pthread_mutex_t {
	int isLocked;
	int mutexattr;
    my_pthread_t owningThread;
    TidQueue waitingThreads;
} my_pthread_mutex_t;

/* define your data structures here: */

// Feel free to add your own auxiliary data structures


/* Function Declarations: */

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield();

/* terminate a thread */
void my_pthread_exit(void *value_ptr);

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr);

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);

#endif

#endif
