#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <pthread.h>
#include "osqueue.h"

typedef enum {
    ShouldNotWait = 0,
    ShouldWait = 1
} waitingForTasks;

typedef struct thread_pool {
    // array that holds the threads in the pool
    pthread_t* threads;
    // queue that holds the tasks
    OSQueue* queue;
    int size;

    // flags
    int destroy_pool;
    int should_wait;
    int can_add_task;

    // mutex
    pthread_mutex_t task_lock;
    pthread_mutex_t queue_lock;
    pthread_mutex_t empty_lock;
    pthread_mutexattr_t mutex_error_check;
    pthread_cond_t wait_for_new_task;
} ThreadPool;

typedef struct task {
    void(*execute)(void*);
    void* params;
} Task;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
