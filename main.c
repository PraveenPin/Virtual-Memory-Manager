#include <stdio.h>
#include "my_pthread_t.h"
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>

#define SIZE 50000
my_pthread_mutex_t mutex, mutex2;
int global_var1 = 0, global_var2 = 0;
typedef struct dummyStruct {
	long long int i;
	long long int j;
	long long int k;
	long long int l;
	long long int m;
	char c;
} dummy;

/***
 * busyWait - Function to mimic sleep() as sleep() wakes on alarm
 * @param 	i 	int 	Approx. duration of wait
 * @return 	null
 */
void busyWait(int i) {
	int j = 21474;
	i = i < 0 ? 1 : i;
    int k = j*j;
	while (k>0) {
        k--;
	}
    
}

int threadFunc1(void*g) {
    printf("Thread 1 is trying to lock the mutex\n");
	int i;
    for(i = 0; i < 2; i++){
        global_var1++;
        busyWait(1);
    }

	dummy *ptrs = (dummy*)malloc(SIZE*sizeof(dummy));
	for(i = 0; i < SIZE; i++){
        (ptrs + i)->i = 11;
	}
    int j=1;
	while( j < SIZE){
        printf("I-%d val ->%d\t",j,(ptrs + j)->i);
        j = j*10;
	}
    printf("\n");

	my_pthread_yield();

	free(ptrs);
    global_var2++;
    return 11;
}

void threadFunc2() {
    int i;
    for(i = 0; i < 2 ; i++) {
        busyWait(2);
    }
    global_var1++;

    dummy * ptrs[SIZE];
	for(i = 0; i < SIZE; i++){
		ptrs[i] = malloc(sizeof(dummy));
        printf("New malloc pointer fot thread 2 %p of size %zd\n",ptrs[i],sizeof(dummy));
        ptrs[i]->i = 11;
        ptrs[i]->j = 11;
        ptrs[i]->k = 11;
        ptrs[i]->l = 11;
        ptrs[i]->m = 11;
	}

	my_pthread_yield();

	for(i = 0; i < SIZE; i++){	
        printf("Freeing malloc pointer fot thread 2 %p\n",ptrs[i]);	
		free(ptrs[i]);
	}
    printf("Thread  2 EXITING!!!!!!!!\n");
}

int threadFunc3() {
    int i;
    for(i = 0; i < 3 ; i++) {
        busyWait(3);
        global_var2++;
    }
    global_var1++;
    
    int **ptrs = malloc(sizeof(int)*SIZE);
	for(i = 0; i < SIZE; i++){
		ptrs[i] = malloc(sizeof(int));
		*ptrs[i] = i;
	}

	for(i = 0; i < SIZE; i++){
		free(ptrs[i]);
	}

    printf("Thread  3 is done!\n");
    return 777;

}

void threadFunc4() {
	int i;
    //my_pthread_mutex_lock(&mutex2);
    for(i = 0; i < 3 ; i++) {
        busyWait(4);
        //printf("This is the fourth Thread 4\n");
    }
    global_var2++;
    //my_pthread_mutex_unlock(&mutex2);
}

void threadFunc(){
    printf("Generic thread function has started\n");
    for(int i = 0; i< 3;i++){
        busyWait(0);
    }
    printf("Generic thread function has completed\n");
}

int main(int argc, const char * argv[]) {
	struct timeval start, end;
	float delta;
	gettimeofday(&start, NULL);
	my_pthread_t *t1 = malloc(sizeof(my_pthread_t));
	my_pthread_t *t2 = malloc(sizeof(my_pthread_t));
    my_pthread_t t3,t4;
    // my_pthread_mutex_init(&mutex, NULL);
    // my_pthread_mutex_init(&mutex2, NULL);
    int *retVal1, *retVal2, *retVal3, *retVal4;
    // Create threads
    my_pthread_create(t1, NULL, &threadFunc1,NULL);
    my_pthread_create(t2, NULL, &threadFunc1,NULL);
    // my_pthread_create(&t3, NULL, &threadFunc3,NULL);
    // my_pthread_create(&t4, NULL, &threadFunc4,NULL);

    printf("Thread id -> %d %d %d %d\n",*t1,*t2,t3,t4);

    my_pthread_join(*t1, &retVal1);
    my_pthread_join(*t2, &retVal2);
    // my_pthread_join(t3, &retVal3);
    // my_pthread_join(t4, &retVal4);
    printf("Retvals - %d %d %d %d\n",retVal1,retVal2,retVal3, retVal4);
    printf("Global variables - %d %d\n",global_var1,global_var2);
    
    // my_pthread_mutex_destroy(&mutex);
    // my_pthread_mutex_destroy(&mutex2);
    gettimeofday(&end, NULL);
    delta = (((end.tv_sec  - start.tv_sec)*1000) + ((end.tv_usec - start.tv_usec)*0.001));
    printf("Execution time in Milliseconds: %f\n",delta);    
    printf("Ending main\n");
    return 0;
}
