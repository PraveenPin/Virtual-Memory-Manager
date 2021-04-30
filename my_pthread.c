// File:	my_pthread.c
// Date:	3/19/2021
// Author: Omkar Dubas, Praveen Pinjala, Shikha Vyaghra
// username of iLab:
// iLab Server: ilab1

#include <signal.h>
#include "my_pthread_t.h"
#include "memory_manager.h"

#define CANNOT_DESTORY_MUTEX_ERROR -2
#define DOES_NOT_HAVE_MUTEX_LOCK -3
#define NO_THREAD_ERROR -4
#define CANNOT_JOIN_ERROR -5
#define BASE_TIME_QUANTA 25

struct itimerval timer_val;
struct sigaction act_timer;
unsigned int threadCount=0;
TCB* running;
static Queue queue[NUMBER_OF_LEVELS];
static Queue waitingQueue, finishedQueue;
static int totalCyclesElapsed = 0;
static long timeSinceLastMaintenance = 0;
int lengthOfLeastPriorityQueue = 0;
extern int algorithm_used;

/*List Implementation*/
int addToTidQueue(unsigned int tid, TidQueue *tidQueue){
    if(tidQueue->front == 0){
        tidQueue->front = myallocate((sizeof(struct ListNode)), __FILE__,__LINE__, LIBRARYREQ);
        tidQueue->front->tid = tid;
        tidQueue->back = tidQueue->front;
        tidQueue->front->next = 0;
        tidQueue->back->next = 0;
    }
    else{
        tidQueue->back->next = myallocate((sizeof(struct ListNode)), __FILE__,__LINE__, LIBRARYREQ);
        tidQueue->back = tidQueue->back->next;
        tidQueue->back->tid = tid;
        tidQueue->back->next = 0;
    }
    
    return 1;
}

int isThisThreadInWaitingForMutex(unsigned int tid, TidQueue *waitingThreads){
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
		mydeallocate(tempNode, __FILE__, __LINE__, LIBRARYREQ);
        tempNode=buf;
    }
}


void stateOfTidQueue(TidQueue *waitingThreads){
    struct ListNode *tempNode = waitingThreads->front;
    if(tempNode != NULL){
        printf("Waiting thread IDs on mutex: \t");
    }
    while(tempNode != NULL){
        printf("%d\t",tempNode->tid);
        tempNode = tempNode->next;
    }
    printf("\n");
}


/*Queue implementation*/
int addToQueue(TCB *thread, Queue *queue){
    if(queue->front == 0){
        queue->front = myallocate((sizeof(struct Node)), __FILE__,__LINE__, LIBRARYREQ);
        queue->front->thread = thread;
        queue->back = queue->front;
        queue->front->next = 0;
        queue->back->next = 0;
    }
    else{
        queue->back->next = myallocate((sizeof(struct Node)), __FILE__,__LINE__, LIBRARYREQ);
        queue->back = queue->back->next;
        queue->back->thread = thread;
        queue->back->next = 0;
    }
    
    return 1;
}

int removeFromQueue(Queue *queue, TCB **thread){
    if(queue->front == 0){
        return 0;
    }
    if(queue->front != queue->back){
        *thread = queue->front->thread;
        struct Node *tempNode = queue->front;
        queue->front = queue->front->next;
		mydeallocate(tempNode, __FILE__, __LINE__, LIBRARYREQ);
    }
    else{
        *thread = queue->front->thread;
		mydeallocate(queue->front, __FILE__, __LINE__, LIBRARYREQ);
        queue->front = 0;
        queue->back = 0;
    }
    return 1;
}

int isQueueEmpty(Queue *queue){
    if(queue->front == 0){
        return 1;
    }
    return 0;
}

void stateOfQueue(Queue *queue){
    struct Node *tempNode = queue->front;
    if(tempNode != NULL){
        printf("Queue %d ->\t",queue->front->thread->priority);
    }
    while(tempNode != NULL){
        printf("%d\t",tempNode->thread->id);
        tempNode = tempNode->next;
    }
    printf("\n");
}

void deleteAParticularNodeFromQueue(my_pthread_t tid, Queue *queue, TCB **thread){
    struct Node *tempNode = queue->front;
    struct Node *prevNode = NULL;
    while(tempNode != NULL && tempNode->thread->id != tid){
        prevNode = tempNode;
        tempNode = tempNode->next;
    }
    //printf("queue->front:%d queue->back:%d tempNode:%d prevNode: %d",queue->front, queue->back, tempNode, prevNode);
    if(tempNode != NULL && tempNode->thread->id == tid){        
        *thread = tempNode->thread;
        
        if(queue->front == queue->back){
            queue->front = 0;
            queue->back = 0;
        }
        else{
            if(tempNode == queue->front){
                queue->front = tempNode->next;
            }
            else if(tempNode == queue->back){
                queue->back = prevNode;
                queue->back->next = NULL;
            }
            else{             
                prevNode->next = tempNode->next;
            }
        }
    }
    else{
        *thread = NULL;
    }
}

TCB* findThreadById(my_pthread_t id, Queue *someQueue){
	struct Node* node = someQueue->front;
	while(node != NULL){
		if (node->thread->id == id){
			return node->thread;
		}
		node = node->next;
	}
	return NULL;
}

TCB* findThreadByIdInMLFQ(my_pthread_t id){
	int i;
	for (i = 0; i< NUMBER_OF_LEVELS; i++){
		struct Node *node = queue[i].front;
		while(node != NULL){
			if (node->thread->id == id){
				return node->thread;
			}
			node = node->next;		
		}
	}
	return NULL;
}

void shiftFromWaitingToReadyQueue(my_pthread_mutex_t *mutex) {
	struct ListNode *tempNode = mutex->waitingThreads.front;
	while(tempNode != NULL){
		TCB *waitingThread;
		// printf("Deleting waiting thread %d from waiting queue\n",tempNode->tid);
		deleteAParticularNodeFromQueue(tempNode->tid,&waitingQueue,&waitingThread);
		if(waitingThread != NULL){
			// printf("Moving Thread %d to Queue %d\n",waitingThread->id,waitingThread->priority);
			addToQueue(waitingThread, &queue[waitingThread->priority]);
		}
		tempNode = tempNode->next;
	}
}

void enableInterrupts(void){
    sigset_t newSignal;
    sigemptyset(&newSignal);
    sigaddset(&newSignal, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &newSignal, NULL);
};

void disableInterrupts(void){
    sigset_t newSignal;
    sigemptyset(&newSignal);
    sigaddset(&newSignal, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &newSignal, NULL);
};

int findMaxPriorityQueue(){
	int i;
	for(i = 0;i < NUMBER_OF_LEVELS;i++){
		if(!isQueueEmpty(&queue[i])){
			return i;
		}
	}
	return -1;
}

int inheritPriority(){
	// printf("******* Executing Priority Inheritance *******\n");
	struct Node *tempNode = waitingQueue.front;
	while(tempNode != NULL){
		if(tempNode->thread->mutex_acquired_thread_id > -1 && tempNode->thread->mutex_acquired_thread_id != running->id){
			TCB* lockAcquiredThread = findThreadById(tempNode->thread->mutex_acquired_thread_id, &queue[NUMBER_OF_LEVELS -1]);
			if(lockAcquiredThread != NULL && lockAcquiredThread->hasMutex && tempNode->thread->priority < lockAcquiredThread->priority){
					TCB *removedThread;
					deleteAParticularNodeFromQueue(lockAcquiredThread->id, &queue[NUMBER_OF_LEVELS - 1],&removedThread);
					if(removedThread != NULL){
						// printf("Removed Thread %d with priority %d by thread %d\n",removedThread->id, removedThread->priority,tempNode->thread->id);
						removedThread->priority = tempNode->thread->priority;
						// printf("Moving Thread %d from Lowest Queue to Queue %d\n",removedThread->id, removedThread->priority);					
						addToQueue(removedThread,&queue[removedThread->priority]);
						// stateOfQueue(&queue[0]);
					}
			}
		}
		tempNode = tempNode->next;
	}
	return NO_THREAD_ERROR;
}

void scheduleMaintenance(){
	// printf("*********************** Calling Maintenance ***********************\n");
	
	inheritPriority();
	// printf("******* Executing Ageing *******\n");
	
	struct Node *prevNode = NULL,*tempNode = queue[NUMBER_OF_LEVELS - 1].front;
	while(tempNode != NULL){
		TCB* currentThread = tempNode->thread;
		struct timespec currentTime;
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&currentTime);

		double starvingMilliSecs,starvingSecs;
		starvingSecs = (double)(currentTime.tv_sec - tempNode->thread->finish.tv_sec);
		if(starvingSecs == 0){
			starvingMilliSecs = ((double)(currentTime.tv_nsec - tempNode->thread->finish.tv_nsec))/1000000;
		}
		else if(starvingSecs >= 1){
			starvingSecs = starvingSecs - 1;
			starvingMilliSecs = ((double)((999999999 - tempNode->thread->finish.tv_nsec) + (currentTime.tv_nsec)))/1000000;
		}

		// printf("Thread %d was starving for %lf secs %lf millisecs\n",tempNode->thread->id,starvingSecs,starvingMilliSecs);

		if(starvingMilliSecs >= 200){ //lengthOfLeastPriorityQueue*BASE_TIME_QUANTA*(NUMBER_OF_LEVELS-2)
			currentThread->priority = 0;
			addToQueue(currentThread,&queue[currentThread->priority]);
			// printf("Thread has starved more than the threshold, Inverted Priority of thread %d to 0\n",currentThread->id);
			if(queue[NUMBER_OF_LEVELS - 1].back == queue[NUMBER_OF_LEVELS - 1].front){
				queue[NUMBER_OF_LEVELS - 1].front = 0;
				queue[NUMBER_OF_LEVELS - 1].back = 0;
			}
			else{
				if(tempNode == queue[NUMBER_OF_LEVELS - 1].back){
					queue[NUMBER_OF_LEVELS - 1].back = prevNode;
				}
				else if(tempNode == queue[NUMBER_OF_LEVELS - 1].front){
					queue[NUMBER_OF_LEVELS - 1].front = tempNode->next;
				}
				else{
					prevNode->next = tempNode->next;
				}
			}
			lengthOfLeastPriorityQueue--;
		}
		else{
			prevNode = tempNode;
		}
		tempNode = tempNode->next;
	}

	// printf("Threads in all queues\n");
	// for(int i=0;i<NUMBER_OF_LEVELS;i++){
	// 	stateOfQueue(&queue[i]);
	// }
}

void initTimerInterrupt(int i){

	timer_val.it_value.tv_sec = 0;
	timer_val.it_value.tv_usec = i*1000;
	timer_val.it_interval.tv_usec= i*1000;
	timer_val.it_interval.tv_sec = 0;
	
	if(setitimer(ITIMER_VIRTUAL, &timer_val,NULL) == -1){
		printf(stderr,"Error calling setitimer for thread %d\n", running->id);
		printf("Error in Setting Timer\n");
	}
}

void scheduler(int sig){
	
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&(running->finish));
	// disable interrupts
	disableInterrupts();
	totalCyclesElapsed++;
	long currentTimeSlice = BASE_TIME_QUANTA*(running->priority + 1), nextTimeSlice = 0;

	if(running == NULL){
		printf("No running thread\n");
		exit(1);
	}
	if(running->firstCycle){
		running->firstCycle = 0;
	}
	else{
		double millisecs,secs;
		secs = (double)(running->finish.tv_sec - running->resume.tv_sec);
		if(secs == 0){
			millisecs = ((double)(running->finish.tv_nsec - running->resume.tv_nsec))/1000000;
		}
		else if(secs >= 1){
			secs = secs - 1;
			millisecs = ((double)((999999999 - running->resume.tv_nsec) + (running->finish.tv_nsec)))/1000000;
		}
		running->timeSpentInSeconds += secs;
		running->timeSpentInMilliSeconds += millisecs;
		running->totalTimeInSecs += secs;
		running->totalTimeInMilliSecs += millisecs;
	}

	timeSinceLastMaintenance += currentTimeSlice;
	//check for maintenance
	if(timeSinceLastMaintenance >= 1000){
		//call maintenance cycle
		timeSinceLastMaintenance = 0;
		scheduleMaintenance();
	}

	if(algorithm_used ==3){
		updateFreeListForSwapOut(running->id);
	}
	
	if(running->state == FINISHED){
		//we have to decide whther to free this or not
		int nextPreferredQueue = findMaxPriorityQueue();
				
		// printf("Time spent by exiting thread secs->%lf\tmillisec->%lf\n",running->totalTimeInSecs,running->totalTimeInMilliSecs);

		if(nextPreferredQueue == -1){
			printf("All the queue are empty\n");
		}
		else{
			TCB* nextThreadToRun;
			removeFromQueue(&queue[nextPreferredQueue],&nextThreadToRun);
			nextTimeSlice = BASE_TIME_QUANTA*(nextPreferredQueue + 1);
			if(nextThreadToRun->firstCycle == 1){
				clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&(nextThreadToRun->start));
			}
			// printf("Swapping threads %d with %d\n", running->id, nextThreadToRun->id);
			running = nextThreadToRun;
			running->state = RUNNING;
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&(nextThreadToRun->resume));	
			initTimerInterrupt(nextTimeSlice);		
			setcontext(nextThreadToRun->context);
		}
	}
	else{	
		TCB* oldThread = running;
		if(oldThread->state == RUNNING){
			oldThread->state = READY;
			if(oldThread->timeSpentInMilliSeconds >= BASE_TIME_QUANTA*(oldThread->priority + 1)){
				// printf("Decreasing the priority of the thread %d\t to %d\n",oldThread->id,oldThread->priority);
				//push to low level queue
				if(oldThread->priority != (NUMBER_OF_LEVELS - 1)){
					oldThread->priority+=1;
					oldThread->timeSpentInSeconds = 0;
					oldThread->timeSpentInMilliSeconds = 0;	
					if(oldThread->priority == (NUMBER_OF_LEVELS - 1)){
						lengthOfLeastPriorityQueue++;
					}
				}				
			}
			addToQueue(oldThread,&queue[oldThread->priority]);

			if(isQueueEmpty(&queue[oldThread->priority])){
				printf("Problem with the queue or thread\n ");
				printf(stderr,"Problem with adding current thread to ready queue\n");
			}
		}
		// stateOfQueue(&queue[oldThread->priority]);

		int nextPreferredQueue = findMaxPriorityQueue();

		if(nextPreferredQueue == -1){
			printf("All the queue are empty\n");
		}
		else{
			TCB* nextThreadToRun;
			removeFromQueue(&queue[nextPreferredQueue],&nextThreadToRun);
			nextTimeSlice = BASE_TIME_QUANTA*(nextPreferredQueue + 1);
			if(nextThreadToRun->firstCycle == 1){
				clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&(nextThreadToRun->start));
			}
			// printf("Swapping threads %d with %d\n", oldThread->id, nextThreadToRun->id);
			running = nextThreadToRun;
			running->state = RUNNING;
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&(nextThreadToRun->resume));
			initTimerInterrupt(nextTimeSlice);
			if(swapcontext(oldThread->context, nextThreadToRun->context) == -1){
				printf(stderr,"Insufficient stack space left\n");
			}
		}
		
	}
	enableInterrupts();
}

void setupSignal(){
	act_timer.sa_handler= scheduler;
	sigemptyset(&act_timer.sa_mask);
	act_timer.sa_flags = SA_RESTART; //0
	sigemptyset(&act_timer.sa_mask);
	sigaddset(&act_timer.sa_mask,SIGVTALRM);
	if(sigaction(SIGVTALRM,&act_timer,NULL)){
		printf(stderr, "Error in sigaction \n");
		printf("************** Error in sigaction ************** \n");
		exit(1);
	}
}	

void Start_Thread(void *(*start)(void *), void *arg){
	void *retVal = start((void *)arg);
	my_pthread_exit(retVal);
}

void freeThread(TCB *threadToFree){
	printf("Freeing up space of thread - %d\n",threadToFree->id);
	if(threadToFree != NULL){
		mydeallocate(threadToFree->stack, __FILE__, __LINE__, LIBRARYREQ);
		mydeallocate(threadToFree->context, __FILE__, __LINE__, LIBRARYREQ);
		mydeallocate(threadToFree, __FILE__, __LINE__, LIBRARYREQ);
	}
}

/* create a new thread */
int my_pthread_create(my_pthread_t * tid, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
		
	if(threadCount == 0){
		//creating main user thread
		
		TCB *mainThread = (TCB*)myallocate(sizeof(TCB), __FILE__,__LINE__, LIBRARYREQ);
		
		if(mainThread == NULL){
			printf("Failure to allocate memory for main thread\n");
			return -1;
		}
		mainThread->id = threadCount++;
		
		mainThread->context = (ucontext_t*)myallocate(sizeof(ucontext_t), __FILE__,__LINE__, LIBRARYREQ);
		if(mainThread->context == NULL){
			printf("Failure to allocate memory for mainThread context\n");
			return -1;
		}
		

		if(getcontext(mainThread->context) == -1){
			printf("Failure to initialise execution context\n");
			return -1;
		}
		mainThread->waiting_id = -1;
		*tid = mainThread->id;
		mainThread->state = RUNNING;
		mainThread->stack = mainThread->context->uc_stack.ss_sp;
		mainThread->mutex_acquired_thread_id = -1;
		mainThread->hasMutex = 0;
		mainThread->firstCycle = 1;
		mainThread->front = NULL;
		mainThread->priority = 0;
		mainThread->timeSpentInMilliSeconds = 0;
		mainThread->timeSpentInSeconds = 0;
		mainThread->totalTimeInMilliSecs = 0;
		mainThread->totalTimeInSecs = 0;
		mainThread->created.tv_sec = 0;
		mainThread->created.tv_nsec = 0;
		mainThread->start.tv_sec = 0;
		mainThread->start.tv_nsec = 0;
		mainThread->resume.tv_sec = 0;
		mainThread->resume.tv_nsec = 0;
		mainThread->finish.tv_sec = 0;
		mainThread->finish.tv_nsec = 0;
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&(running->created));
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&(running->start));
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&(running->resume));
		running = mainThread;

		//setting the timer interrupt and signal handler
		setupSignal();
		initTimerInterrupt(BASE_TIME_QUANTA);
	}

	TCB *thread = (TCB*)myallocate(sizeof(TCB), __FILE__,__LINE__, LIBRARYREQ);
	if(thread == NULL){
		printf(stderr, "Failure to allocate memory for TCB of thread\n");
		return -1;
	}

	//Intialise TCB
	thread->context = (ucontext_t*)myallocate(sizeof(ucontext_t),__FILE__,__LINE__,LIBRARYREQ);
	if(thread->context == NULL){
		printf(stderr, "Failure to allocate memory for thread context\n");
		return -1;
	}

	thread->state = READY;
	thread->id = threadCount++;
	thread->waiting_id = -1;
	thread->mutex_acquired_thread_id = -1;
	thread->hasMutex = 0;
	thread->firstCycle = 1;
	thread->front = NULL;
	// thread->retVal = NULL;
	thread->priority = 0;
	thread->timeSpentInMilliSeconds = 0;
	thread->timeSpentInSeconds = 0;
	thread->totalTimeInMilliSecs = 0;
	thread->totalTimeInSecs = 0;
	thread->created.tv_sec = 0;
	thread->created.tv_nsec = 0;
	thread->start.tv_sec = 0;
	thread->start.tv_nsec = 0;
	thread->resume.tv_sec = 0;
	thread->resume.tv_nsec = 0;
	thread->finish.tv_sec = 0;
	thread->finish.tv_nsec = 0;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&(running->created));

	thread->stack =  myallocate(STACK_SIZE,__FILE__,__LINE__,LIBRARYREQ);
	if(thread->stack == NULL){
		printf(stderr,"Cannot allocate memory for stack\n");
		return -1;
	}

	if(getcontext(thread->context) == -1){
		printf(stderr,"Failure to initialise execution context\n");
		return -1;
	}

	thread->context->uc_stack.ss_sp = thread->stack;
	thread->context->uc_stack.ss_size = STACK_SIZE;
	thread->context->uc_stack.ss_flags = 0;
	thread->context->uc_link = 0;

	*tid = thread->id;

	addToQueue(thread, &queue[thread->priority]);
	// printf("Front thread at queue %d\n",queue[thread->priority].back->thread->id);
	// printf("\n** Adding Thread %d to Queue: %p** \n", thread->id,queue[thread->priority].back);

	makecontext(thread->context, (void(*)(void))Start_Thread,2,function, arg);

	return thread->id;
};

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {

	disableInterrupts();
	// printf("Thread %d is giving up CPU state: %d\n",running->id, running->state);
	
	if(running->state == WAITING){
		// printf("Adding the current thread to wait queue\n");
		//add to waiting list
		addToQueue(running,&waitingQueue);	
		// stateOfQueue(&waitingQueue);
	}
	else if( running->state == FINISHED){
		// printf("Adding the current finished thread to finished queue\n");
		removePages(running->id);
		addToQueue(running,&finishedQueue);
	}
	// printf("Raising a signal to call scheduler\n");
	enableInterrupts();
	raise(SIGVTALRM);	
	return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
	disableInterrupts();
	printf("\n** Thread %d exiting with return value %d**\n",running->id, (int)value_ptr);
	if(running->id == 0){ //main thread
		exit(0);
	}
	running->state = FINISHED;
	if(running->waiting_id >= 0){
		if(value_ptr != NULL && running->retVal != NULL){
			*running->retVal = value_ptr;
		}
		else{
			running->retVal = NULL;
		}
		//loop the waiting Queue for thread and set state to ready
		TCB* waitingThread;
		//deleting the waiting thread in waiting queue and adding into ready queue
		deleteAParticularNodeFromQueue(running->waiting_id,&waitingQueue,&waitingThread);
		if(waitingThread == NULL){
			printf("Exiting - Some problem in accessing waiting thread %d\n", running->waiting_id);
			exit(1); //add some error
		}
		running->waiting_id = -1;
		// stateOfQueue(&waitingQueue);
		waitingThread->state = READY;
		addToQueue(waitingThread,&queue[waitingThread->priority]);
		// stateOfQueue(&queue[waitingThread->priority]);
	}
	my_pthread_yield();
	enableInterrupts();
};

/* wait for thread termination */
int my_pthread_join(my_pthread_t tid, void **value_ptr) {
	disableInterrupts();
	if(tid > MAX_THREADS){
		return NO_THREAD_ERROR;
	}
	// printf("Searching in all queues for Thread %d\n",tid);
	TCB* threadToWaitOn = findThreadByIdInMLFQ(tid);
	if(threadToWaitOn == NULL){
		threadToWaitOn = findThreadById(tid, &waitingQueue);
		if(threadToWaitOn == NULL){
			threadToWaitOn = findThreadById(tid, &finishedQueue);
			if(threadToWaitOn == NULL){
				printf(stderr,"No thread with id %d to wait on in any queue\n",tid);
				return NO_THREAD_ERROR;
			}
			else{
				printf("Found Thread %d in finished queue\n",tid);
				if(threadToWaitOn->retVal != NULL && value_ptr != NULL){
					*value_ptr = *threadToWaitOn->retVal;
					freeThread(threadToWaitOn);
				}
				return 0;
			}
		}
	}
	if(running == NULL){
		printf(stderr,"Problem with accessing current thread\n");
		return -1;
	}
	if(running->id == tid){
		printf(stderr,"Cannot wait on itself\n");
		return -1;
	}
	printf("Thread %d started waiting on thread %d\n",running->id,threadToWaitOn->id);
	if(threadToWaitOn->waiting_id >= 0 ){
		printf("Some other thread has joined this thread %d\n",tid);
		return CANNOT_JOIN_ERROR;
	}
	threadToWaitOn->waiting_id = running->id;
	threadToWaitOn->retVal = value_ptr;
	running->state = WAITING;
	my_pthread_yield();
	//interrupt enable
	enableInterrupts();
	return 0;
};

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	mutex = ( my_pthread_mutex_t *) myallocate(sizeof(my_pthread_mutex_t), __FILE__,__LINE__, LIBRARYREQ);
	if(mutex == NULL){
		printf("Error allocating memory for Mutex block \n");
		return -1;
	}
	if(mutex->isLocked == 1)
		return -1;
	
    mutex->isLocked = 0;
    mutex->mutexattr = mutexattr;
    mutex->owningThread = -1;
    printf("Mutex created");
    return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	// printf("Check is mutex locked? %d\n", mutex -> isLocked);
    while(1){
        if(mutex -> isLocked == 1) {
            if(mutex->owningThread == running -> id) {
                printf("Mutex is already locked by running thread\n");
                return 0;
            }
            running -> mutex_acquired_thread_id = mutex->owningThread; 
            running->state = WAITING;
            addToTidQueue(running-> id, &(mutex -> waitingThreads));
			printf(" ************* Thread %d waiting for mutex lock on thread %d ************* \n",running->id, running->mutex_acquired_thread_id);
            my_pthread_yield();
        }
        else {
            printf("Mutex is not locked, locking mutex by thread %d\n", running ->id );
            mutex -> isLocked = 1;
			running->mutex_acquired_thread_id = -1;
            mutex->owningThread = running -> id;
			mutex->waitingThreads.front = NULL;
			mutex->waitingThreads.back = NULL;
            if(mutex->owningThread  == -1 ) {
                printf(stderr,"Some error occurred in allocating mutex to thread");
                return -1;
            }
	        running->hasMutex++;
            return 0;
        }
    }
	return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
    if(mutex == NULL){
        my_pthread_mutex_init(&mutex, NULL);
    }

	printf("Unlocking the mutex by thread %d \n",running->id);
    
    if(mutex->isLocked == 1 && running->hasMutex && mutex->owningThread == running->id) {
        shiftFromWaitingToReadyQueue(mutex);
        mutex->isLocked = 0;
        running->hasMutex--;
        mutex->owningThread = -1;
        emptyTidQueue(&(mutex -> waitingThreads));
		return 0;
    }

	printf("Mutex unlock failed by Thread %d\n",running->id);
	return DOES_NOT_HAVE_MUTEX_LOCK;
};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	if(!mutex->isLocked && mutex->owningThread == -1){
    	mutex = NULL;
		return 0;
	}
	printf("Cannot destroy Mutex, lock is acquired by some thread\n");
    return CANNOT_DESTORY_MUTEX_ERROR;
};
