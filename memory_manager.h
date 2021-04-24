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

#define MAIN_MEMORY 85*4*1024

#define PAGE_SIZE sysconf( _SC_PAGE_SIZE)

#define TOTAL_PAGES 85
#define TOTAL_FILE_PAGES 80

#define THREAD_PAGES 4 
#define MEMORY_TABLE_PAGES 1
#define SWAP_TABLE_PAGES 1
#define LIBRARY_PAGES 79


// #define MAIN_MEMORY 8*1024*1024

// #define PAGE_SIZE sysconf( _SC_PAGE_SIZE)

// #define TOTAL_PAGES 2048
// #define TOTAL_FILE_PAGES 4096

// #define THREAD_PAGES 1024 //change this is 1020 if you uncomment the below line
// #define SHARED_PAGES 4

// #define MEMORY_TABLE_PAGES 2
// #define SWAP_TABLE_PAGES 8
// #define LIBRARY_PAGES 1014


typedef enum {LIBRARYREQ, THREADREQ} requestType;
typedef enum {FALSE, TRUE} Bool;

typedef struct metaDataBlock {
  struct metaDataBlock * next;
  size_t size;
} MDBlock;


typedef struct pageTableEntry{
  int tid;
  unsigned short index;
  Bool useBit;
} PTEntry;

typedef struct freeNode{
  PTEntry node;
  struct freeNode *next;
} freeVictimList;

void * myallocate(size_t size, char *  file, int line, requestType retType);

void mydeallocate(void * freeThis, char * file, int line, requestType retType);
#endif 
