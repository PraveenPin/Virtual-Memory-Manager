CC = gcc
CFLAGS = -g -c
AR = ar -rc
RANLIB = ranlib


Target: my_pthread.a

my_pthread.a: my_pthread.o memory_manager.o
	$(AR) libmy_pthread.a my_pthread.o memory_manager.o
	$(RANLIB) libmy_pthread.a

my_pthread.o: my_pthread_t.h memory_manager.h
	$(CC) -pthread $(CFLAGS) my_pthread.c memory_manager.c

clean:
	rm -rf testfile *.o *.a
