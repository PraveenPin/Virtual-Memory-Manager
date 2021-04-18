#include <sys/mman.h>
#include <signal.h>
#include "memory_manager.h"

static char *memory;
invertedPTEntry *invertedPageTable;
int *invertedSwapTable;
int memory_manager_init = 0;
FILE *swapFile;

int availablePages = 0;
int PAGE_SIZE = 0;
int TOTAL_PAGES = 0;
int SHARED_PAGES = 0;
int SWAP_PAGES = 0;
int KERNEL_PAGES = 0;

float SHARED_SPACE = 0.25;
float KERNEL_SPACE = 0.25;
float PAGE_SPACE = 0.5;


#undef malloc(x)
#undef free(x)

struct sigaction memoryAccessSigAction;



void setBit(int *A, int k) {
	A[k / 32] |= 1 << (k % 32);  // Set the bit at the k-th position in A[i]
}

int testBit(int *A, int k) {
	return ((A[k / 32] & (1 << (k % 32))) != 0);
}

void clearBit(int *A, int k) {
	A[k / 32] &= ~(1 << (k % 32));
}

void initializeMemoryManager() {
	if (memory_manager_init == 0) {

		PAGE_SIZE = sysconf(_SC_PAGE_SIZE);
		availablePages = MAIN_MEM_SIZE / PAGE_SIZE;
		TOTAL_PAGES = availablePages * PAGE_SPACE;
		SHARED_PAGES = availablePages * SHARED_SPACE;
		KERNEL_PAGES = availablePages * KERNEL_SPACE;
		KERNEL_PAGES--; // The last page in Main memory will be reserved for Buffer Space.
		SWAP_PAGES = (SWAP_FILE_SIZE / PAGE_SIZE);

		//	Initialize Inverted Page Table and mem-align the pages
		invertedPageTable = (invertedPTEntry*) malloc(availablePages * sizeof(invertedPTEntry));

		memory = (char*) memalign(PAGE_SIZE, MAIN_MEM_SIZE);

		int i = 0, j = 0;
		// Initialize Inverted Swap Table
		invertedSwapTable = (int *) malloc(SWAP_PAGES / 32);
		for (i = 0; i < SWAP_PAGES; i++) {
			clearBit(invertedSwapTable, i);
		}

		// Init Inverted Page Table
		for (i = 0; i < availablePages; i++) {

			invertedPageTable[i].tid = -1;
			invertedPageTable[i].isAllocated = 0;
			invertedPageTable[i].maxFree = PAGE_SIZE - sizeof(pmData);
			pmData *pageAddr = (void *) (memory + (i * PAGE_SIZE));
			pageAddr->free = 1;
			pageAddr->size = PAGE_SIZE - sizeof(pmData);
			pageAddr->isMaxFreeBlock = 1;
		}

		//Init Thread Page Table
		threadPageTable = (PTEntry **) malloc(sizeof(PTEntry *));
		for (i = 0; i < MAX_THREADS; i++) {
			threadPageTable[i] = NULL;
		}

		//init swap space
		swapFile = fopen(SWAP_FILE, "w+");

		ftruncate(fileno(swapFile), SWAP_FILE_SIZE);
		close(fileno(swapFile));
		memory_manager_init = 1; /*Setting Memory Manager Initialised as true*/

		// Protect all pages
		mprotect(memory, PAGE_SIZE * TOTAL_PAGES, PROT_NONE);

		// Register Handler
		memoryAccessSigAction.sa_flags = SA_SIGINFO;
		sigemptyset(&memoryAccessSigAction.sa_mask);
		memoryAccessSigAction.sa_sigaction = memoryAccessHandler;

		if (sigaction(SIGSEGV, &memoryAccessSigAction, NULL) == -1) {
			printf("Fatal error setting up signal handler\n");
			exit(EXIT_FAILURE); 
		}
	}
}

void *myallocate(size_t size, char* fileName, int lineNumber, int flag){
    // int alloc_complete = 0;
    int i = j = 0;
    int oldPage = 0;
    if(flag != THREADREQ){
        printf("******** called by library ********\n");
        return NULL;
        // return specialAllocate(size,KERNEL_REGION);
    }
    else{

    }

}