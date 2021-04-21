#include "memory_manager.h"
#include "my_pthread_t.h"

int availablePages = 0;
int PAGE_SIZE = 4096;
int TOTAL_FRAMES = 1596;
int TOTAL_PAGES = 5692;
int SHARED_PAGES = 0;
int SWAP_PAGES = 0;
int KERNEL_PAGES = 0;

float SHARED_SPACE = 0.25;
float KERNEL_SPACE = 0.25;
float PAGE_SPACE = 0.5;

extern TCB* running;
void *memory;
pgmData **frameTable;
static int initialiseMemory = 1;
MemoryEntry *head;
MemoryEntry *end;
void *tempVarForSwappingMemory;
void *osMemoryPtr;
void *shallocMemPtr;
void * swapTemp;
int swap_fd;
int pageCounter = 0;
pgmData *currentPageMetaData;
int memoryFlag;
MemoryEntry *shallocHead = NULL;
MemoryEntry *shallocEnd = NULL;
int count = 0;
static const int myIdentifier = 1234567890;

void swapHandler(int sig, siginfo_t *si, void *unused) {
	
	/*
	 * Check page table to see if the page being accessed belongs to the current thread.
	 * 		if so, change that page's mprotect to allow read/write: mprotect( memory + frame_number * 4096, 4096, PROT_READ | PROT_WRITE);
	 * 		else, find the page they want and swap pages
	 */
	
	printf("Calling Swap handler for thread %d\n",running->id);
	
	void * segAddress = (void *) si->si_addr;
	int difference = (int)(segAddress - memory);
	int frame = difference / PAGE_SIZE;
	
	if (difference < 0 || difference > TOTAL_FRAMES*PAGE_SIZE) {
		printf("Page trying to access does not exist\n");
		exit(EXIT_FAILURE);
	}

	printf("Trying to access frame number %d\n",frame);
	
	if (frame > running->mallocFrame) {
		exit(EXIT_FAILURE);
	}
	
	if (frameTable[frame]->tid != running->id) {
		int i;
		pgmData *tempPtr = running->firstPage;
		for (i = 0; i < frame; i++) {
			tempPtr = tempPtr->next;
		}
		swapPages(frame, tempPtr->pageFrame);
		mprotect(memory, 6537216, PROT_NONE);
	}
	mprotect(memory + (frame * PAGE_SIZE), PAGE_SIZE,PROT_READ | PROT_WRITE);
	return;
}

int freeMemoryCount() {
	MemoryEntry *trav = head;
	int retVal = 0;
	
	while(trav != NULL) {
		if (trav->free) {
			retVal += trav->size;
		}
		trav = trav->next;
	}
	
	return retVal;
}


void * mymalloc(unsigned int x, char * file, int line, void * memPtr, int size) {
	/*
	 * If nothing has been allocated yet, this creates and initializes the first MemoryEntry struct which is pointed to by head and middle.
	 */
	if (head == NULL) {
		head = (MemoryEntry*)memPtr;
		head->size = size - sizeof(MemoryEntry);
		head->identifier = myIdentifier;
		head->free = 1;
		head->next = NULL;
		head->prev = NULL;
		end = head;
	}
	
	MemoryEntry *trav = head;		//Creates a pointer to traverse the MemoryEntry list.
	if(x < 100) {					//If the size requested is below 100 bytes, we will allocate the memory from the left.
		while(trav != NULL) {		//Traverses the MemoryEntry list until it finds a free spot. If not found, memory cannot be allocated.
			
			if(trav->free == 1) {
				if(trav->size >= x) {
					if(trav->size > (x + sizeof(MemoryEntry))) {		//If there's space to create a new mementry as well as the requested space...
						MemoryEntry* temp = (MemoryEntry*)(((char *)trav) + sizeof(MemoryEntry) + x);	//Creates a new mementry after the allocated space.
						
						temp->identifier = myIdentifier;								//Initializes the values for the new mementry. Re-assigns pointers.
						temp->size = trav->size - sizeof(MemoryEntry) - x;
						temp->free = 1;
						temp->next = trav->next;
						temp->prev = trav;
						if(trav->next != NULL) {
							trav->next->prev = temp;
						}
						
						trav->size = x;									//Change values for the old pointer.
						trav->free = 0;
						trav->next = temp;
						
						if(trav == end){
							end = temp;
						}
												
						return trav+1;					//Returns the pointer to the requested space.
					}
					else {
						trav->free = 0;						
						return trav+1;					//Returns the pointer to the requested space.
					}
				}
				else {
					trav = trav->next;
				}
			}
			else {
				trav = trav->next;
			}
		}
	}else {								//Larger requests will be allocated from the right. 
		trav = end->next;
		while(trav != NULL){			//Checks for free space to the right of the middle.
			if(trav->free == 1) {
				if(trav->size >= x) {
					if(trav->size > (x + sizeof(MemoryEntry))) {			//If there's space to create a new mementry as well as the requested space...
						MemoryEntry *temp = (MemoryEntry*)(((char *)trav) + sizeof(MemoryEntry) + x);	//Creates a new mementry after the allocated space.
						
						temp->identifier = myIdentifier;												//Initializes the values for the new mementry. Re-assigns pointers.
						temp->size = trav->size - sizeof(MemoryEntry) - x;
						temp->free = 1;
						temp->next = trav->next;
						temp->prev = trav;
						if(trav->next != NULL) {
							trav->next->prev = temp;
						}
						
						trav->size = x;													//Change values for the old pointer.
						trav->free = 0;
						trav->next = temp;
						
						return trav+1;					//Returns the pointer to the requested space.
					}
					else {
						trav->free = 0;
						return trav+1;					//Returns the pointer to the requested space.
					}
				}
				else {
					trav = trav->next;
				}
			}
			else {
				trav = trav->next;
			}
		}
		trav = end;
		while(trav != NULL) {				//Traverses left from the middle looking for free space.
			if(trav->free == 1) {
				if(trav->size >= x) {
					if(trav->size > (x + sizeof(MemoryEntry))) {			//If there's space to create a new mementry as well as the requested space...
						MemoryEntry *temp = (MemoryEntry*)((char *)trav + sizeof(MemoryEntry) + (trav->size - x - sizeof(MemoryEntry)));
						
						temp->identifier = myIdentifier;												//Initializes the values for the new mementry. Re-assigns pointers.
						temp->size = x;
						temp->free = 0;
						temp->next = trav->next;
						temp->prev = trav;
						if(trav->next != NULL) {
							trav->next->prev = temp;
						}
						
						trav->size = trav->size - x - sizeof(MemoryEntry);			//Change values for the old pointer.
						trav->free = 1;
						trav->next = temp;
						return temp+1;					//Returns the pointer to the requested space.
					}
					else {
						trav->free = 0;
						return trav+1;					//Returns the pointer to the requested space.
					}
				}
				else {
					trav = trav->prev;
				}
			}
			else {
				trav = trav->prev;
			}
		}
	}
	//If you reach here, the space could not be allocated.
	printf("mymalloc could not allocate the requested space at line %d of file %s.\n", line, file);
	return NULL;
}


void* myallocate(unsigned int x, char *file, int line, requestType reqType){
	memoryFlag=1;
	mprotect(memory,6537216, PROT_READ | PROT_WRITE);

	void* returnPtr;
	pgmData *newpgmData;

	//First call to intialise
	if(initialiseMemory){
		initialiseMemory = 0;

		//Registering signal handler
		struct sigaction sig;
		sig.sa_flags = SA_SIGINFO;
		sigemptyset(&sig.sa_mask);
		sig.sa_sigaction = swapHandler;
		if(sigaction(SIGSEGV, &sig, NULL) == -1){
			printf("Fata error setting up signal handler\n");
			exit(EXIT_FAILURE);			
		}

		
		memory = memalign(sysconf(_SC_PAGESIZE),MAIN_MEM_SIZE);
		printf("Memory starts at %p\n",memory);
		//required identifier to setup swap file

		
		osMemoryPtr = memory + 1601*(PAGE_SIZE);
		shallocMemPtr = memory + TOTAL_FRAMES*(PAGE_SIZE); //1596 frames in main memory

		frameTable = (pgmData**)myallocate((sizeof(pgmData*))*TOTAL_FRAMES,NULL,0,LIBRARYREQ);
		printf("Frame Table starts at %p\n",frameTable);
		memoryFlag=1;
		mprotect(memory,6537216,PROT_READ | PROT_WRITE);

	}


	printf("Checking for request size %d or %f pages of request type %d at line %d of file %s\n",(int)x,(float)x/PAGE_SIZE,reqType,line,file);

	//If current thread has a page
	if(reqType == LIBRARYREQ){
		void* temp = osMemoryPtr;
		osMemoryPtr += (int)x;
		mprotect(memory,6537216,PROT_NONE);
		return temp;
	}

	//if request is greater than page size
	if(x > PAGE_SIZE){

		//calculate how many pages are needed for the request
		int i;
		int numOfPages = (x+32)/PAGE_SIZE;

		if((x+32)%PAGE_SIZE > 0){
			numOfPages++;
		}



	}

	if(running->firstPage != NULL){
		pgmData *tempPtr = frameTable[running->mallocFrame];

		//if page is not in frame then swap it
		if(tempPtr->tid != running->id){
			//Swap in the required page
			tempPtr = running->firstPage;
			while(tempPtr != NULL){
				tempPtr = tempPtr->next;
			}

			swapPages(running->mallocFrame, tempPtr->pageFrame);
			mprotect(memory,6537216,PROT_READ | PROT_WRITE);
		}
		currentPageMetaData = tempPtr;

		//if page is a part of a big chunk
		if(currentPageMetaData->frontToMetaData != NULL){
			int i;
			pgmData *swapPtr = currentPageMetaData->frontToMetaData;
			for(i = currentPageMetaData->frontToMetaData->pageFrame; i < currentPageMetaData->pageFrame;i++){
				tempPtr = frameTable[i];
				if(tempPtr != swapPtr){
					swapPages(tempPtr->pageFrame,swapPtr->pageFrame);
					mprotect(memory,6537216,PROT_READ | PROT_WRITE);
				}
				swapPtr = swapPtr->next;
			}
			
		}

		head = currentPageMetaData->head;
		end = currentPageMetaData->end;


	}
	else{
		//if running thread does not have its first page yet

		//check for total pages are exceeded or not
		if(pageCounter < TOTAL_PAGES){
			swapPages(0,-1);
			mprotect(memory,6537216,PROT_READ | PROT_WRITE);
			newpgmData = (pgmData*)myallocate(sizeof(pgmData), NULL, 0, LIBRARYREQ);
			memoryFlag= 1;
			mprotect(memory,6537216,PROT_READ | PROT_WRITE);
			newpgmData->pageFrame = 0;
			newpgmData->tid = running->id;
			newpgmData->next = NULL;
			newpgmData->head = NULL;
			newpgmData->end = NULL;
			newpgmData->frontToMetaData = NULL;
			newpgmData->numPages = 1;
			running->firstPage = newpgmData;
			frameTable[0] = newpgmData;
			currentPageMetaData = newpgmData;
		}
		else{
			printf("Maximum page limit reached \n");
			return NULL;
		}
		head = NULL;
		end = NULL;
	}

	returnPtr = mymalloc(x,file,line,memory+running->mallocFrame*PAGE_SIZE,PAGE_SIZE);
	currentPageMetaData->head= head;
	currentPageMetaData->end = end;
	currentPageMetaData->freePageMemory = freeMemoryCount();

	//if there is no space in page to satisfy the request
	if(returnPtr == NULL){

		printf("Current page is full\n");
		//checking that any of thread's previous pages have room left for request
		pgmData *tempPtr = running->firstPage;
		int counter = 0;
		while(tempPtr != NULL){
			//does the current page satisfy the request 
			if(tempPtr-> freePageMemory >= (int)x){
				currentPageMetaData = tempPtr;

				//if the page is in swap file, then swap it in


				if(currentPageMetaData->frontToMetaData != NULL){
					int i;
					pgmData *tempBlock, *swapPtr = currentPageMetaData->frontToMetaData;
					for(i = currentPageMetaData->frontToMetaData->pageFrame; i < currentPageMetaData->pageFrame;i++){
						tempBlock = frameTable[i];
						if(tempBlock != swapPtr){
							swapPages(tempBlock->pageFrame,swapPtr->pageFrame);
							mprotect(memory,6537216,PROT_READ | PROT_WRITE);
						}
						swapPtr = swapPtr->next;
					}
					
				}
				printf("Malloc at frame %d which has %d space free\n",currentPageMetaData->pageFrame,currentPageMetaData->freePageMemory);
				head = currentPageMetaData->head;
				end = currentPageMetaData->end;

				returnPtr = mymalloc(x,file,line,0,0);
				printf("Return pointer - %p\n",returnPtr);
				if(returnPtr != NULL){
					currentPageMetaData->head = head;
					currentPageMetaData->end = end;
					currentPageMetaData->freePageMemory = freeMemoryCount();
					mprotect(memory,6537216,PROT_NONE);
					memoryFlag = 0;
					return returnPtr;
				}

			}
			counter++;
			tempPtr = tempPtr->next;
		}


		//None of the thread's existing pages could satisfy the request, allocate new page
		if(pageCounter < TOTAL_PAGES && running->mallocFrame < TOTAL_FRAMES){
			running->mallocFrame++;

			//swaps the page after current page to free space for contiguous pages
			swapPages(running->mallocFrame,-1);
			mprotect(memory,6537216,PROT_READ | PROT_WRITE);
			newpgmData->pageFrame = running->mallocFrame;
			newpgmData->tid = running->id;
			newpgmData->next = NULL;
			newpgmData->head = NULL;
			newpgmData->end = NULL;
			newpgmData->numPages = 1;
			frameTable[running->mallocFrame] = newpgmData;
			frameTable[running->mallocFrame-1]->next = newpgmData;
			currentPageMetaData = newpgmData;
		}
		else{
			return NULL;
		}
		head = NULL;
		end = NULL;

		returnPtr = mymalloc(x,file,line,memory+(running->mallocFrame*PAGE_SIZE),PAGE_SIZE);
		currentPageMetaData->head = head;
		currentPageMetaData->end = end;
		currentPageMetaData->freePageMemory = freeMemoryCount();

	}
	mprotect(memory,6537216,PROT_NONE);
	memoryFlag = 0;
	return returnPtr;
}

void swapPages(int srcFrame, int destFrame) {
	
	count++;
	
	memoryFlag = 1;
	mprotect(memory, 6537216, PROT_READ | PROT_WRITE);
	
	int swapIndex;
	if (destFrame == -1) {

		if(pageCounter < 1597){
			
			
			//Swap to new frame from free_queue_head
			memcpy(memory + pageCounter*PAGE_SIZE, memory + srcFrame * PAGE_SIZE, PAGE_SIZE);
			
			//Update page table
			//as we know we always allocated the 0th page to new page request and swap it with page next to the last page
			if (frameTable[srcFrame] != NULL) {
				frameTable[srcFrame]->pageFrame = pageCounter;
				frameTable[pageCounter] = frameTable[srcFrame];
				frameTable[srcFrame] = NULL;
			}
		}else{
			//frames are filled, use swap file
		}
		pageCounter++;
	} 
	else {
		
		if (destFrame < 1597) {
			//swapping one frame with other
			memcpy(tempVarForSwappingMemory, memory + srcFrame * PAGE_SIZE, PAGE_SIZE);
			memcpy(memory + srcFrame * PAGE_SIZE, memory + destFrame * PAGE_SIZE, PAGE_SIZE);
			memcpy(memory + destFrame * PAGE_SIZE, tempVarForSwappingMemory, PAGE_SIZE);
			
			//Update page table
			pgmData *temp = frameTable[srcFrame];
			frameTable[srcFrame] = frameTable[destFrame];
			frameTable[destFrame] = temp;
			frameTable[srcFrame]->pageFrame = srcFrame;
			frameTable[destFrame]->pageFrame = destFrame;
		} else {
			//swap file identifier
		}
	}
	memoryFlag = 0;
}

void myfree(void *x, char *file, int line){
	if(((char*)x < (char*)(head) + sizeof(MemoryEntry)) || ((char*)x > (char*)(head) + 4999)){
		printf("Pointer %p given by line %d of file %s is out of bounds\n",x,line,file);
		return -1;
	}

	MemoryEntry *freePtr = (MemoryEntry*)((char*)x - sizeof(MemoryEntry));
	if (freePtr->identifier == myIdentifier){
		if(freePtr->free == 1){
			printf("This pointer %p at line %d of file %s is already freed\n",x,line,file);
			return -1;
		}
		else{
			freePtr->free = 1;
		}
	}
	else{
		printf("Identifier did not match-> Cannot free memory %p at line %d of file %s, allocated by other malloc\n",x,line,file);
		return -1;
	}

	/*If the previous MemoryEntry is free, then combine the current with previous */
	if(freePtr->prev != NULL && freePtr->prev->free == 1){
		if(freePtr->prev != NULL){
			freePtr->prev->next = freePtr->next;
		}
		if(freePtr->next != NULL){
			freePtr->next->prev = freePtr->prev;
		}
		freePtr->prev->size = sizeof(MemoryEntry)+freePtr->size;
		if(freePtr == end){
			end  = freePtr-> prev;
		}
		freePtr = freePtr->prev;
	}

	/*If the next MemoryEntry is free, then combine the current with next */
	if(freePtr->next != NULL && freePtr->next->free == 1){
		if(freePtr->prev != NULL){
			freePtr->prev->next = freePtr->next;
		}
		if(freePtr->next != NULL){
			freePtr->next->prev = freePtr->prev;
		}
		freePtr->size = sizeof(MemoryEntry)+freePtr->size;
		if(freePtr == end){
			end  = freePtr;
		}
	}
	return 0;

}

int mydeallocate(void *x, char *file, int line, requestType reqType){
	memoryFlag = 1;
	mprotect(memory, 6537216, PROT_READ | PROT_WRITE);

	int memoryOffset = (int)(x - memory);
	int pageIndex = (memoryOffset)/PAGE_SIZE;
	int i;

	//if page is in frame table
	if(frameTable[pageIndex]->tid == running->id){
		currentPageMetaData = frameTable[pageIndex];
		head = currentPageMetaData->head;
		end = currentPageMetaData->end;

		//check myfree return value for errors
		myfree(x,file,line);
		currentPageMetaData->head = head;
		currentPageMetaData->end = end;
		currentPageMetaData->freePageMemory = freeMemoryCount();
	}
	else{
		//page is not in page table, so swap it into page table

	}
	mprotect(memory,6537216,PROT_NONE);
	memoryFlag = 0;
	return 0;
}	




