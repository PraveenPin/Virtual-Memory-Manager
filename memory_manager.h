#ifndef MEMORY_MANAGER_H_
#define MEMORY_MANAGER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define malloc(x) myallocate(x, __FILE__, __LINE__, THREADREQ) 
#define free(x) mydeallocate(x, __FILE__, __LINE__, THREADREQ) 
// #define shalloc(x) myshalloc(x, __FILE__, __LINE__) //x is a size

#define MAIN_MEMORY 8*1024*1024

#define PAGE_SIZE sysconf( _SC_PAGE_SIZE)

#define TOTAL_PAGES 2048
#define TOTAL_FILE_PAGES 4096

#define THREAD_PAGES 1024

#define MEMORY_TABLE_PAGES 2
#define SWAP_TABLE_PAGES 8
#define LIBRARY_PAGES 1014


typedef enum {LIBRARYREQ, THREADREQ} requestType;
typedef enum {FALSE, TRUE} Bool;

typedef struct metaDataBlock {
  struct metaDataBlock * next;
  size_t size;
} MDBlock;


typedef struct pageTableEntry{
  int tid;
  unsigned short index;
  Bool useBit: 1;
} PTEntry;

struct freeNode{
    int frame;
    struct freeNode *next;
};

typedef struct{
    struct freeNode *front;
    struct freeNode *back;
}FreeVictimList;

int addToFreeList(int frame, FreeVictimList *queue);

int removeFromFreeList(FreeVictimList *queue, int *swapOutFrame);

int isFreeListEmpty(FreeVictimList *queue);

void stateOfFreeList(FreeVictimList *queue);

void deleteAParticularNodeFromFreeList(int frame, FreeVictimList *queue);

void * myallocate(size_t size, char *  file, int line, requestType retType);

void mydeallocate(void * freeThis, char * file, int line, requestType retType);
#endif 
