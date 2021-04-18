#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include "my_pthread_t.h"

#ifndef MEMORY_MANAGER_H_
#define MEMORY_MANAGER_H_

// #define THREADREQ 

#define malloc(x) myallocate(x,__FILE__,__LINE__,THREADREQ)
#define free(x) mydeallocate(x,__FILE__,__LINE__,THREADREQ)

#define MAIN_MEM_SIZE 8*1024*1024
#define SWAP_FILE_SIZE 16*1024*1024
#define SWAP_FILE "swap_file.swp"

void protect_all_pages();

typedef struct pageTableEntry{
    uint in_mem;
    uint isDirty;
    uint memPageNumber;
    uint swapPageNumber;
    struct pageTableEntry *next;
} PTEntry;

/* Thread Page Table -> Array of Linked List, where each list contains all pages information that a thread owns.*/
PTEntry **threadPageTable;

typedef struct invertedPageTableEntry{
    my_pthread_t tid;
    uint isAllocated;
    uint pageNum;
    uint maxFree;    
} invertedPTEntry;

typedef struct pageMetadata{
    uint free;
    uint isMaxFreeBlock;
    uint size;
} pmData;

#endif 
