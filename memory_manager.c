#include <signal.h>
#include <errno.h>
#include "memory_manager.h"
#include "my_pthread_t.h"

extern int errno;
/* Pointers to the start of each section of memory */
char * memory = NULL;
// char * s_mem = NULL;
char * lib_memory = NULL;

PTEntry * MemoryPageTableFront = NULL;
PTEntry * SwapFilePageTableFront = NULL;

/* Malloc "front" pointers for library and shalloc requests */
MDBlock * libFront = NULL;
// MDBlock * sharedFront = NULL;

/* File descriptor for the swapfile */
int swapfd;
extern TCB *running;
extern unsigned int threadCount;
unsigned int victimThreshold = 2;


/* TESTING-ONLY FUNCTIONS*/

/* __printPT()__
 *	Prints out the first n entries of the memory page table.
 *	Args:
 *		- int howMany - the number of entries to print
 *	Returns:
 *		- N/A
 */
void printPageTable(int howMany){
  if(MemoryPageTableFront == NULL)
    return;
  PTEntry *ptr = MemoryPageTableFront;
  int i = 0;
  printf("   Memory Page Table    \n");
  for(i = 0; i < howMany; i++){
    printf("i: %d - tid: %d, index - %d, useBit - %d\n", i, (*ptr).tid, (*ptr).index, ptr->useBit);
    ptr += 1;
  }
  printf("\n");
  if(SwapFilePageTableFront == NULL)
    return;
  ptr = SwapFilePageTableFront;
  printf("   Swap Page Table    \n");
  for(i = 0; i < howMany; i++){
    printf("i: %d - tid: %d, index - %d\n", i, (*ptr).tid, (*ptr).index);
    ptr += 1;
  }
  printf("\n");
}

void printMemory(){
  MDBlock *start = memory;
  printf("Blocks Start -\t");
  while(start != NULL){
    printf("%d\t",start->size);
    start = start->next;
  }
  printf("- Blocks End\n");
}


int getLRUFrameFromMemoryPageTable(){
  int frame = 0, tempIndex = 0;
  while(1){

    if(MemoryPageTableFront[tempIndex].useBit == FALSE){
      frame = tempIndex;
      return frame;
      // break;
    }
    else if(MemoryPageTableFront[tempIndex].useBit == TRUE){
      MemoryPageTableFront[tempIndex].useBit = FALSE;
    }

    if(tempIndex >= THREAD_PAGES-1){
        tempIndex = 0;
    }
    else{
      tempIndex++;
    }
  }    
}

/* HELPER METHODS */

/* __removePages()__
 *	Clears all page table metadata for a given tid
 *	Args:
 *		- unsigned int tid - the tid to delete all pages of
 *	Returns:
 *		- N/A
 */
void removePages(unsigned int tid){
  PTEntry * ptr = MemoryPageTableFront;

  printf("Removing pages for thread %d\n",tid);

  int i = 0;
  for(i = 0; i < THREAD_PAGES; i++){
    if((*ptr).tid == tid){
      (*ptr).tid = -1;
      (*ptr).index = 0;
      ptr->useBit = FALSE;      
    }
    ptr += 1;
  }

  ptr = SwapFilePageTableFront;
  for(i = 0; i < TOTAL_FILE_PAGES; i++){
    if((*ptr).tid == tid){
      (*ptr).tid = -1;
      (*ptr).index = 0;
      ptr->useBit = FALSE;
    }
    ptr += 1;
  }
}

/* __protectAll()__
 *	Calls mprotect on all thread pages
 *	Args:
 *		- N/A
 *	Returns:
 *		- N/A
 */
void protectAll(){
  // Memprotect memory space - FOR THREAD PAGES ONLY
  char * memProt = memory;
  int i = 0;
  for(i = 0; i < THREAD_PAGES; i++){
    mprotect(memProt, PAGE_SIZE, PROT_NONE);  //disallow all accesses of address buffer over length pagesize
    memProt += PAGE_SIZE;
  }
}

/* __internalSwapper()__
 *	Swaps two pages and un-mprotects in. (MEM->MEM only)
 *	Args:
 *		- unsigned int in - the index of the page being swapped in
 *    - unsigned int out - the index of the page being swapped out
 *	Returns:
 *		- N/A
 */
void internalSwapper(unsigned int in, unsigned int out){
  if(in >= THREAD_PAGES || out >= THREAD_PAGES){
    // ERROR
    return;
  }

  // Find & allow access to memory locations
  char * inPtr = memory + in*PAGE_SIZE;
  char * outPtr = memory + out*PAGE_SIZE;
  mprotect(inPtr, PAGE_SIZE, PROT_READ | PROT_WRITE); //allow read and write to address buffer over length pagesize
  mprotect(outPtr, PAGE_SIZE, PROT_READ | PROT_WRITE); //allow read and write to address buffer over length pagesize

  // Set up temps
  char tempP[PAGE_SIZE];
  char * temPtr = tempP;
  PTEntry tempPI;

  // Copy actual data
  temPtr = memcpy(temPtr, outPtr, PAGE_SIZE);
  outPtr = memcpy(outPtr, inPtr, PAGE_SIZE);
  inPtr = memcpy(inPtr, temPtr, PAGE_SIZE);

  // Protect what was swapped out
  if(out != in)
    mprotect(inPtr, PAGE_SIZE, PROT_NONE);  //disallow all accesses of address buffer over length pagesize

  //mark the use bit
  MemoryPageTableFront[out].useBit = TRUE;
  MemoryPageTableFront[in].useBit = TRUE;

  // Change Page Table
  tempPI = MemoryPageTableFront[out];
  MemoryPageTableFront[out] = MemoryPageTableFront[in];
  MemoryPageTableFront[in] = tempPI;
}

void memoryToSwapFileSwapper(unsigned int in, unsigned int out){
  printf("*******************************************************************************************\n");
  if(in >= THREAD_PAGES || out >= TOTAL_FILE_PAGES){
    // ERROR
    return;
  }

  // Find & allow access to memory locations
  char * inPtr = memory + in*PAGE_SIZE;
  mprotect(inPtr, PAGE_SIZE, PROT_READ | PROT_WRITE); //allow read and write to address buffer over length pagesize

  // Set up temps
  char tempP[PAGE_SIZE];
  char * temPtr = tempP;
  PTEntry tempPI;

  // Read from swapfd
  int total = out*PAGE_SIZE;
  while(total > 0){
    total = total - lseek(swapfd, total, SEEK_CUR);
  }

  total = PAGE_SIZE;
  while(total > 0){
    total = total - read(swapfd, temPtr, total);
  }

  lseek(swapfd, 0, SEEK_SET); // Reset swapfd to front of swapfile

  // Write in data to swap file
  total = out*PAGE_SIZE;
  while(total > 0){
    total = total - lseek(swapfd, total, SEEK_CUR);
  }

  total = PAGE_SIZE;
  while(total > 0){
    total = total - write(swapfd, inPtr, total);
  }

  lseek(swapfd, 0, SEEK_SET); // Reset swapfd to front of swapfile

  // Copy from buffer
  inPtr = memcpy(inPtr, temPtr, PAGE_SIZE);


  // Alter Page tables
  tempPI = SwapFilePageTableFront[out];
  SwapFilePageTableFront[out] = MemoryPageTableFront[in];
  MemoryPageTableFront[in] = tempPI;

  //mark the swapped in page
  MemoryPageTableFront[in].useBit = TRUE;
  SwapFilePageTableFront[out].useBit = FALSE;
}

/* __createMeta()__
 *	Given the info needed, populates the given space with metadata and returns
 *    a pointer to the actual chunk of memory directly after it
 *	Args:
 *		- void * start - pointer to where the metadata is going
 *    - int newSize - size of the allocated chunk
 *    - MDBlock * newNextBlock - pointer to the next metadata block
 *	Returns:
 *		- void * - a pointer to the beginning of the associated memory
 */
void * createMeta(void * start, int newSize, MDBlock * newNextBlock){
  MDBlock * placeData = (MDBlock *) start;
  placeData->size = newSize;
  placeData->next = newNextBlock;

  return (void *) (placeData + 1);
}


/* SEGMENTATION FAULT HANDLER */
static void SegFaultHandler(int sig, siginfo_t *si, void *unused) {
  disableInterrupts();
  printf("Page fault occured !!!! Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);

  unsigned int tid;
  char * accessed = si->si_addr;

  if(running == NULL){ // HASN'T MALLOCED, must be actual segfault
    // actual segfault
    printf("Actual seg fault !! Returning to next location\n",(long) si->si_addr);
    enableInterrupts();
    return;
  }

  tid = running->id;
  
  if(accessed < memory || accessed > memory + THREAD_PAGES*PAGE_SIZE){ 
    printf("Trying to access memory which is out of bounds (Main Memory)\n");
    // actual segfault
    enableInterrupts();
    exit(-1);
    return;
  }

  int index = 1;
  for(index = 1; index <= THREAD_PAGES; index++){
    if(accessed < memory + index*PAGE_SIZE){
      index = index-1;
      break;
    }
  }

  // Check if we need a swap
  if(MemoryPageTableFront[index].tid == tid && MemoryPageTableFront[index].index == index){ 
    printf("Accessing its own page => granting access\n");
    mprotect(memory + index*PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE); // Un-mempotect and go
    enableInterrupts();
    return;

  }
  else { // Find the right page and swap it in
    // In memory
    printf("Trying in-memory swapping for page number %d\n",index);
    int swapIndex = 0;
    for(swapIndex = 0; swapIndex < THREAD_PAGES; swapIndex++){
      if(MemoryPageTableFront[swapIndex].tid == tid && MemoryPageTableFront[swapIndex].index == index){
        printf("Found page number %d in memory page table and swapping with page number %d\n",index,swapIndex);
        internalSwapper(swapIndex, index);
        enableInterrupts();
        return;
      }
    }

    printf("Starting 2-nd chance PRA\n");

    //2nd chance circular clock
    //find victim which is not used recently
    //change the index 
    int frame = 0,tempIndex = 0;
    int oldCounter = 0;
    while(1){
      if(MemoryPageTableFront[tempIndex].tid == -1){
        frame = tempIndex;
        break;
      }
      else{
        if(MemoryPageTableFront[tempIndex].useBit == FALSE){
          frame = tempIndex;
          break;
        }
        else if(MemoryPageTableFront[tempIndex].useBit == TRUE){
          MemoryPageTableFront[tempIndex].useBit = FALSE;
        }
      }
        if(tempIndex>=THREAD_PAGES-1){
          tempIndex=0;
        }
        tempIndex++;
    }

    printf("Found LRU page in frame %d\n",frame);


    printf("Checking in swap file for swapping for page number %d\n",index);
    // In swapfile
    swapIndex = 0;
    for(swapIndex = 0; swapIndex < TOTAL_FILE_PAGES; swapIndex++){
      if(SwapFilePageTableFront[swapIndex].tid == tid && SwapFilePageTableFront[swapIndex].index == index){
        printf("Swapping in page number %d from swap file into frame number %d\n",SwapFilePageTableFront[swapIndex].index,frame);
        memoryToSwapFileSwapper(frame, swapIndex);
        enableInterrupts();
        return;
      }
    }

    printf("Page Number %d is out of bounds\n",index);
    if(swapIndex == THREAD_PAGES){
      // actual segfault
      enableInterrupts(); 
      exit(-1); 
    }
    printf("Aborted\n",index);
    exit(-1);
  }
}


/* MAIN FUNCTIONS */
void * t_myallocate(size_t size, char *  file, int line, char * memStart, size_t memSize, MDBlock ** frontPtr){
  if(size < 1 || size + (sizeof(MDBlock)) > memSize) {
		printf("ERROR: Can't malloc < 0 or greater then %d byte - File: %s, Line: %d\n", (memSize - (sizeof(MDBlock))), file, line);
    return NULL;
	}

  // Place as new front
	if((*frontPtr) == NULL) {
    (*frontPtr) = (MDBlock *) memStart;
      printf("Placing at the front\n");
    return createMeta((void *) memStart, size, NULL);
  }

  //Place before front
  if( ((char *) (*frontPtr)) - memStart >= size + (sizeof(MDBlock))){
    MDBlock * temp = (*frontPtr);
    (*frontPtr) = (MDBlock *) memStart;
      printf("Placing at the front\n");
    return createMeta((void *) memStart, size, temp);
  }

  // Place in the Middle
  MDBlock * ptr = (*frontPtr);
  while((*ptr).next != NULL){

    // Put it in the space between
    if ( ((char *) (*ptr).next) - ( ((char *) ptr) + (sizeof(MDBlock)) + (*ptr).size) >= size + (sizeof(MDBlock))) {
      MDBlock * temp = (*ptr).next;
      (*ptr).next = (MDBlock *) ( ((char *) ptr) + (sizeof(MDBlock)) + (*ptr).size);
      printf("Placing in the middle\n");
      return createMeta((void *) (*ptr).next, size, temp);
    }

    ptr = (*ptr).next;
  }

  // Place at the end
  if ( (memStart + memSize) - ( ((char *) ptr) + (sizeof(MDBlock)) + (*ptr).size) >= size + (sizeof(MDBlock))) {
    (*ptr).next = (MDBlock *) ( ((char *) ptr) + (sizeof(MDBlock)) + (*ptr).size);
    printf("Placing at the end\n");
    return createMeta((void *) (*ptr).next, size, NULL);
  }

  //printf("ERROR: Not enough space to malloc. - File: %s, Line: %d\n", file, line);
  return NULL;
}

/* __myallocate()__
 *	Deals with all of the paging aspects so t_myallocate can assume contiguous space
 *	Args:
 *		- size_t size - the size of the memory space that is being requested
 *    - char *  file/int line - directives for error displaying
 *    - int reqType - THREADREQ/LIBRARYREQ
 *	Returns:
 *		- void * - a pointer to the beginning of the associated memory
 *    - NULL - if it cannot be done
 */
void * myallocate(size_t size, char *  file, int line, requestType reqType){
  disableInterrupts();

  unsigned int tid;
  MDBlock ** currFront;
  int i;
  void * ret;

  printf("Request size ----------------------------> %d\n",size);

  // Has memory been created yet?
  if(memory == NULL){
    memory = (char *) memalign(PAGE_SIZE, MAIN_MEMORY);
    if(memory == NULL){
      //ERROR
      enableInterrupts();
      return NULL;
    }

    // Memprotect memory space
    protectAll();

    printf("Main memory is initialised %p\n",memory);

    // Create swapfile
    swapfd = open("memory.dat", O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
    if(swapfd == -1){
      printf("Error opening swap file with %s\n",strerror(errno));
      enableInterrupts();
      return NULL;
    }

    // Reset swapfd to front  pf swapfile
    lseek(swapfd, 0, SEEK_SET);

    // Size the swapfile
    i = 0;
    for(i = 0; i < TOTAL_FILE_PAGES; i++){
      int total = PAGE_SIZE;
      while(total > 0){
        total = total - write(swapfd, memory + (THREAD_PAGES)*PAGE_SIZE, PAGE_SIZE);
      }
    }

    // Reset swapfd to front  pf swapfile
    lseek(swapfd, 0, SEEK_SET);


    // Set other memory pointers
    // s_mem = memory + (THREAD_PAGES)*PAGE_SIZE;
    // lib_memory = memory + (THREAD_PAGES+SHARED_PAGES)*PAGE_SIZE;
	  lib_memory = memory + (THREAD_PAGES)*PAGE_SIZE;
    printf("Library memory is initialised %p\n",lib_memory);

    // Clear Memory Page table space + set table front
    MemoryPageTableFront = (PTEntry *) ((char *)(memory + PAGE_SIZE*(TOTAL_PAGES-MEMORY_TABLE_PAGES)));
    printf("Memory Page Table is initialised %p\n",MemoryPageTableFront);

    PTEntry *temp = MemoryPageTableFront;
    i = 0;
    for(i = 0; i < THREAD_PAGES; i++){
      temp->tid = -1;
      temp->index = 0;
      temp->useBit = FALSE;
      temp = temp + 1;
    }

    // Clear File Page table space + set table front
    SwapFilePageTableFront = (PTEntry *) ((char *)(memory + PAGE_SIZE*(TOTAL_PAGES-MEMORY_TABLE_PAGES-SWAP_TABLE_PAGES)));
    printf("Swap Page Table is initialised %p\n",SwapFilePageTableFront);

    temp = SwapFilePageTableFront;
    for(i = 0; i < TOTAL_FILE_PAGES; i++){
      temp->tid = -1;
      temp->index = 0;
      temp->useBit = FALSE;
      temp = temp + 1;
    }

    // Set up signal handler
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = SegFaultHandler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
    {
        printf("Fatal error setting up signal handler\n");
        exit(EXIT_FAILURE);
    }
  }

  if(reqType == LIBRARYREQ){ // Request from the scheduler
    ret = t_myallocate(size, file, line, lib_memory, LIBRARY_PAGES*PAGE_SIZE, &libFront);
    enableInterrupts();
    return ret;

  } else { //Its a thread
    if(running == NULL){ // Threads have not been created yet
		//call scheduler
			if(threadCount == 0){
			printf("Creating main thread from memory manager\n");
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
			initTimerInterrupt(25);
		}
    }

    // Set variables for rest of method
    tid = running->id;
    currFront = &(running->front);
  }

  // Find thread's total space + Find 1st free page + Find num of free pages
  int threadPagesInMemory = 0; // Checking memory
  int freePagesInMemory = 0;
  PTEntry * firstEmptyPageInMemory = NULL;

  PTEntry * temp = MemoryPageTableFront;
  i = 0;
  for(i = 0; i < THREAD_PAGES; i++){
    if(temp->tid == tid) {
      threadPagesInMemory += 1;
    }

    if(temp->tid == -1){
      if(firstEmptyPageInMemory == NULL)
        firstEmptyPageInMemory = temp;
      freePagesInMemory += 1;
    }
    temp = temp + 1;
  }

  printf("Thread %d has %d pages allocated\n", tid,threadPagesInMemory);
  printf("Free pages in main memory %d\n",freePagesInMemory);

  int threadPagesInSwapFile = 0; // Checking swapfile
  int freePagesInSwapFile = 0;
  int firstEmptyPageIndexInSwapFile = 0;
  PTEntry * firstEmptyPageInSwapFile = NULL;

  temp = SwapFilePageTableFront;
  i = 0;
  for(i = 0; i < TOTAL_FILE_PAGES; i++){
    if(temp->tid == tid) {
      threadPagesInSwapFile += 1;
    }

    if(temp->tid == -1){
      if(firstEmptyPageInSwapFile == NULL){
        firstEmptyPageInSwapFile = temp;
        firstEmptyPageIndexInSwapFile = i;
      }
      freePagesInSwapFile += 1;
    }
    temp = temp + 1;
  }

  printf("Thread %d has %d swap pages allocated\n", tid,threadPagesInSwapFile);
  printf("Free pages in swap file %d with first empty page index %d\n",freePagesInSwapFile,firstEmptyPageIndexInSwapFile);

  // Make sure enough space exists for it
  char allow = 'y';
  unsigned int allocationSize = size + (sizeof(MDBlock));
  if((PAGE_SIZE*(THREAD_PAGES-threadPagesInSwapFile-threadPagesInMemory) < allocationSize) &&
      ((PAGE_SIZE*(freePagesInSwapFile+freePagesInMemory)) < allocationSize)){
        printf("Reuested size %d but space left %d\n",allocationSize,(PAGE_SIZE*(freePagesInSwapFile+freePagesInMemory)));
      allow = 'n';
  } // TRY BUT DON'T ALLOW NEW PAGE ALLOCATION

  // If it had no Pages
  if(threadPagesInMemory + threadPagesInSwapFile == 0){
    printf("Allocating first page for thread %d\t",tid);
    if(firstEmptyPageInMemory != NULL){ // Pages exist in memory
      printf("In Memory\n");

      firstEmptyPageInMemory->tid = tid;
      firstEmptyPageInMemory->index = 0;
      firstEmptyPageInMemory->useBit = TRUE;

      threadPagesInMemory = 1;
      freePagesInMemory -= 1;

    } else if (firstEmptyPageInSwapFile != NULL) { // Pages exist in swapfile
      printf("In Swap File\n");

      firstEmptyPageInSwapFile->tid = tid;
      firstEmptyPageInSwapFile->index = 0;

      threadPagesInSwapFile = 1;
      freePagesInSwapFile -= 1;

    } else { // If there are no free pages
      // ERROR
        printf(", But no free pages to allocate\n");
      enableInterrupts();
      return NULL;
    }
  }

  // Try to malloc there
  ret = t_myallocate(size, file, line, memory, PAGE_SIZE*(threadPagesInMemory+threadPagesInSwapFile), currFront);

  printPageTable(5);
  printMemory();

  while(ret == NULL && allow == 'y'){
    if(freePagesInMemory > 0){
      // Look for next free page - in memory
      temp = MemoryPageTableFront;
      for(i = 0; i < THREAD_PAGES; i++){
        if(temp->tid == -1){
          temp->tid = tid;
          temp->index = threadPagesInMemory + threadPagesInSwapFile;
          temp->useBit = TRUE;

          threadPagesInMemory += 1;
          freePagesInMemory -= 1;
          break;
        }
        temp = temp + 1;
      }
    }
    else if(freePagesInSwapFile > 0) {
      //Look for LRU page in memory and swap it with first free page in swapfile 
      //use this 
      int frame = getLRUFrameFromMemoryPageTable();

      printf("Found LRU page frame %d to swap with first free page %d in swap file\n",frame);

      memoryToSwapFileSwapper(frame, firstEmptyPageIndexInSwapFile);

       // Look for next free page - in memory
      temp = MemoryPageTableFront;
      for(i = 0; i < THREAD_PAGES; i++){
        if(temp->tid == -1){
          temp->tid = tid;
          temp->index = threadPagesInMemory + threadPagesInSwapFile;
          temp->useBit = TRUE;

          threadPagesInMemory += 1;
          freePagesInMemory -= 1;
          break;
        }
        temp = temp + 1;
      }


      // Look for next free page - in swapfile
      // temp = SwapFilePageTableFront;
      // for(i = 0; i < TOTAL_FILE_PAGES; i++){
      //   if(temp->tid == -1){
      //     temp->tid = tid;
      //     temp->index = threadPagesInMemory + threadPagesInSwapFile;
      //     threadPagesInSwapFile += 1;
      //     freePagesInSwapFile -= 1;
      //     break;
      //   }
      //   temp = temp + 1;
      // }
    } else {
      // ERROR
      printf("Last Resort");
      enableInterrupts();
      return NULL;
    }

    ret = t_myallocate(size, file, line, memory, PAGE_SIZE*(threadPagesInMemory+threadPagesInSwapFile), currFront);
  }

  enableInterrupts();
  return ret;
}

/* __myshalloc()__
 *	Attempts to obtain space in a shared region so that multiple threads may access it
 *	Args:
 *		- size_t size - the size of the memory space that is being requested
 *    - char *  file/int line - directives for error displaying
 *	Returns:
 *		- void * - a pointer to the beginning of the associated memory
 *    - NULL - if it cannot be done
 */
// void * myshalloc(size_t size, char *  file, int line){
//   disableInterrupts();

//   if(memory == NULL){
//     // ERROR
//     enableInterrupts();
//     return NULL;
//   }

//   void * ret = t_myallocate(size, file, line, s_mem, SHARED_PAGES*PAGE_SIZE, &sharedFront);

//   enableInterrupts();
//   return ret;
// }

int t_mydeallocate(void * freeThis, char * file, int line, MDBlock ** frontPtr, char shalloc){
	if((*frontPtr) == NULL){
    if(shalloc != 's')
      printf("ERROR: No malloced chunks to free - File: %s, Line: %d\n", file, line);
    return -1;

  } else if(freeThis == NULL){
    if(shalloc != 's')
      printf("ERROR: Cannot free a NULL pointer - File: %s, Line: %d\n", file, line);
    return -1;
  }

  // First metaBlock
  if(freeThis == (void *)((*frontPtr) + 1)){
    (*frontPtr) = (*(*frontPtr)).next;
    return 0;
  }

  // Middle + End metaBlocks
  MDBlock * ptr = (*frontPtr);
  while(ptr->next != NULL){
    if(freeThis == (void *)(ptr->next + 1)){
      ptr->next = ptr->next->next;
      return 0;
    }

    ptr = ptr->next;
  }

  if(shalloc != 's')
    printf("ERROR: Cannot free an unmalloced pointer - File: %s, Line: %d\n", file, line);
  return -1;
}

/* __mydeallocate()__
 *	Deals with all of the paging aspects so t_mydeallocate can assume contiguous space
 *	Args:
 *		- void * freeThis - the pointer to the memory to free
 *    - char *  file/int line - directives for error displaying
 *    - int reqType - THREADREQ/LIBRARYREQ
 *	Returns:
 *		- N/A
 */
void mydeallocate(void * freeThis, char * file, int line, requestType reqType){
  disableInterrupts();

  unsigned int tid;
  MDBlock ** currFront;

  // Leave error if memory hasnt been created yet
  if(memory == NULL){
    printf("ERROR: Can't free un-malloced space - File: %s, Line: %d\n", file, line);
    enableInterrupts();
    return;
  }

  // Who called mydeallocate()?
  if(reqType == LIBRARYREQ){ // Request from the scheduler
    t_mydeallocate(freeThis, file, line, &libFront, 'l');
    enableInterrupts();
    return;

  } else { //Its a thread
    if(running == NULL){ // Threads have not been created yet
      printf("ERROR: Can't free un-malloced space - File: %s, Line: %d\n", file, line);
      enableInterrupts();
      return;
    }

    tid = running->id;
    currFront = &((running->front));
  }

  // Deallocate the given pointer if possible
  t_mydeallocate(freeThis, file, line, currFront, 'l');

  enableInterrupts();
}