#ifndef MEMORY_MANAGER_H_
#define MEMORY_MANAGER_H_

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include "my_pthread_t.h"

#define malloc(x) myallocate(x,__FILE__,__LINE__,THREADREQ)
#define free(x) mydeallocate(x,__FILE__,__LINE__,THREADREQ)

#define MAIN_MEM_SIZE 8*1024*1024
#define SWAP_FILE_SIZE 16*1024*1024
#define SWAP_FILE "swap_file"

/* MYMALLOC STRUCTS */
typedef struct MemEntry {
	int size;
	int identifier;
	int free;
	struct MemEntry *next;
	struct MemEntry *prev;
}MemoryEntry;


typedef struct pageMetaData{
    int pageFrame;
    long int tid;
    int numPages;
    int freePageMemory;    
    struct pageMetaData *next;
    struct pageMetaData *frontToMetaData;
    MemoryEntry *head;
    MemoryEntry *end;
}pgmData;

typedef enum {THREADREQ, LIBRARYREQ} requestType;

void* myallocate(unsigned int x, char *file, int line, requestType reqType);
int mydeallocate(void *x, char *file, int line, requestType reqType);

// void lockMemory();
// void *shalloc(size_t size);
void swapPages(int src_frame, int dest_frame);



#endif 
