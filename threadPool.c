// Adi Yanai 209009349
#include "threadPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define PRINT_ERROR write(2, "Error in system call\n", strlen("Error in system call\n"));

/**
 * check the return value,
 * and destroy the threadPool if the return val different from 0
 * @param ret - a return value to check
 * @param threadPool - the threadPool
 */
void checkReturnValue(int ret, ThreadPool* threadPool) {
    if (ret != 0 ) {
        PRINT_ERROR;
        tpDestroy(threadPool,0);
        _exit(EXIT_FAILURE);
    }
}

/**
 * destroy all the mutex
 * @param tp - the threadPool
 */
void destroyMutex(ThreadPool* tp) {
    pthread_mutex_destroy(&(tp->task_lock));
    pthread_mutex_destroy(&(tp->queue_lock));
    pthread_mutex_destroy(&(tp->empty_lock));
    pthread_mutexattr_destroy(&(tp->mutex_error_check));
    pthread_cond_destroy(&(tp->wait_for_new_task));
}

/**
 * empty the tasks queue
 * @param tp - the threadPool
 */
void emptyTheQueue(ThreadPool* tp) {
    int ret;
    ret = pthread_mutex_lock(&(tp->task_lock));
    if (ret != 0) {
        PRINT_ERROR;
        _exit(EXIT_FAILURE);
    }
    while(!osIsQueueEmpty(tp->queue)) {
        Task *task_temp_ptr = (Task*)osDequeue(tp->queue);
        free(task_temp_ptr);
    }

    ret = pthread_mutex_unlock(&(tp->task_lock));
    if (ret != 0) {
        PRINT_ERROR;
        _exit(EXIT_FAILURE);
    }
}

/**
 * free all the dynamic memory allocated and destroyed all the mutex
 * @param tp - the threadPool
 */
void freeResources(ThreadPool *tp) {
    emptyTheQueue(tp);
    destroyMutex(tp);
    free(tp->queue);
    free(tp->threads);
    free(tp);
}

/**
 * run the tasks
 * @param thread_pool - the threadPool
 * @return nothing
 */
void *runTask(void *thread_pool) {
    int ret;
    ThreadPool* tp = (ThreadPool*)thread_pool;
    while (tp->destroy_pool == 0) {
        ret = pthread_mutex_lock(&(tp->task_lock));
        checkReturnValue(ret,thread_pool);

        // if the queue is empty but the user can add tasks to the thread, wait
        if (osIsQueueEmpty(tp->queue) && tp->can_add_task) {
            ret = pthread_cond_wait(&(tp->wait_for_new_task), &(tp->task_lock));
            checkReturnValue(ret, tp);

            // if the queue is empty and the user cannot add tasks to the thread, break
        } else if (osIsQueueEmpty(tp->queue) && !tp->can_add_task) {
            ret = pthread_mutex_unlock(&(tp->task_lock));
            checkReturnValue(ret,thread_pool);
            break;

            // if the queue isn't empty and the user cannot add tasks to the thread
        } else if (!osIsQueueEmpty(tp->queue) && !tp->can_add_task) {
            ret = pthread_mutex_lock(&(tp->empty_lock));
            checkReturnValue(ret,thread_pool);

            // if there is no need to wait to the tasks on the queue, break
            if (tp->should_wait != ShouldWait) {
                ret = pthread_mutex_unlock(&(tp->empty_lock));
                checkReturnValue(ret,thread_pool);

                ret = pthread_mutex_unlock(&(tp->task_lock));
                checkReturnValue(ret,thread_pool);

                break;
            }

            ret = pthread_mutex_unlock(&(tp->empty_lock));
            checkReturnValue(ret,thread_pool);
        }

        ret = pthread_mutex_unlock(&(tp->task_lock));
        checkReturnValue(ret,thread_pool);

        ret = pthread_mutex_lock(&(tp->queue_lock));
        checkReturnValue(ret,thread_pool);

        // if the queue isn't empty pull one task from the queue and run it
        if (!osIsQueueEmpty(tp->queue)) {
            Task* current_task;
            current_task = (Task*)osDequeue(tp->queue);

            ret = pthread_mutex_unlock(&(tp->queue_lock));
            checkReturnValue(ret,thread_pool);

            // run the current task
            current_task->execute(current_task->params);
            // free the dynamic memory allocated of the task
            free(current_task);
        } else {
            ret = pthread_mutex_unlock(&(tp->queue_lock));
            checkReturnValue(ret,thread_pool);
        }


        ret = pthread_mutex_lock(&(tp->empty_lock));
        checkReturnValue(ret,thread_pool);

        // if there is no need to wait to the tasks on the queue, break
        if (tp->should_wait == ShouldNotWait) {
            ret = pthread_mutex_unlock(&(tp->empty_lock));
            checkReturnValue(ret,thread_pool);
            break;
        }

        ret = pthread_mutex_unlock(&(tp->empty_lock));
        checkReturnValue(ret,thread_pool);
    }
}

/**
 * create the mutex
 * @param tp - the threadPool
 */
void createMutex(ThreadPool* tp) {
    // create PTHREAD_MUTEX_ERRORCHECK
    if (pthread_mutexattr_init(&(tp->mutex_error_check)) != 0) {
        PRINT_ERROR;
        free(tp);
        _exit(EXIT_FAILURE);
    }
    if (pthread_mutexattr_settype(&(tp->mutex_error_check), PTHREAD_MUTEX_ERRORCHECK) != 0) {
        pthread_mutexattr_destroy(&(tp->mutex_error_check));
        PRINT_ERROR;
        free(tp);
        _exit(EXIT_FAILURE);
    }
    // initialize all the mutex and conds
    if ((pthread_mutex_init(&(tp->task_lock), &(tp->mutex_error_check)) != 0) ||
        (pthread_mutex_init(&(tp->queue_lock), &(tp->mutex_error_check)) != 0) ||
        (pthread_mutex_init(&(tp->empty_lock), &(tp->mutex_error_check)) != 0) ||
        (pthread_cond_init(&(tp->wait_for_new_task), NULL) != 0 )) {
        PRINT_ERROR;
        pthread_mutexattr_destroy(&(tp->mutex_error_check));
        free(tp);
        _exit(EXIT_FAILURE);
    }
}

/**
 * create the threadPool
 * @param numOfThreads - the number of threads to create
 * @return a threadPool
 */
ThreadPool* tpCreate(int numOfThreads) {
    ThreadPool* tp;

    // initialize the threadPool
    tp = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (tp == NULL) {
        PRINT_ERROR;
        _exit(EXIT_FAILURE);
    }

    // create the mutex
    createMutex(tp);

    // initialize the threadPool size
    tp->size = numOfThreads;

    // initialize the threadPool flags
    tp->can_add_task = 1;
    tp->destroy_pool = 0;
    tp->should_wait = ShouldWait;

    // initialize the threads array
    tp->threads = (pthread_t*)malloc(numOfThreads * sizeof(pthread_t));
    if (tp->threads == NULL) {
        PRINT_ERROR;
        destroyMutex(tp);
        free(tp);
        _exit(EXIT_FAILURE);
    }

    // create queue
    tp->queue = NULL;
    tp->queue = osCreateQueue();
    if (tp->queue == NULL) {
        PRINT_ERROR;
        destroyMutex(tp);
        free(tp->threads);
        free(tp);
        _exit(EXIT_FAILURE);
    }

    // create the threads
    int i;
    for (i = 0; i < numOfThreads; i++) {
        int pt = pthread_create(&(tp->threads[i]), NULL, runTask, (void*)tp);
        if (pt != 0) {
            PRINT_ERROR;
            tpDestroy(tp, 0);
            _exit(EXIT_FAILURE);
        }
    }

    return tp;
}

/**
 * destroy the threadPool.
 * if shouldWaitForTasks=0 wait just for the tasks that are already running
 * if shouldWaitForTasks=1 wait to all the tasks that are running including the ones who waits on the queue
 * @param threadPool - the threadPool
 * @param shouldWaitForTasks - a variable that represent if there is need to wait to all the tasks, also
 *                             those that wait on the queue, or just wait for the tasks that are already running.
 */
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks) {
    int ret;

    // if the pool doesn't destroyed yet
    if (!threadPool->destroy_pool) {
        // the user cannot add tasks to this threadPool anymore
        threadPool->can_add_task = 0;

        ret = pthread_mutex_lock(&(threadPool->task_lock));
        if (ret != 0) {
            PRINT_ERROR;
            freeResources(threadPool);
            _exit(EXIT_FAILURE);
        }

        // signal all the threads to stop wait for new task
        ret = pthread_cond_broadcast(&(threadPool->wait_for_new_task));
        if (ret != 0 ) {
            PRINT_ERROR;
            freeResources(threadPool);
            _exit(EXIT_FAILURE);
        }

        ret = pthread_mutex_unlock(&(threadPool->task_lock));
        if (ret != 0) {
            PRINT_ERROR;
            freeResources(threadPool);
            _exit(EXIT_FAILURE);
        }

        // wait to all the tasks that are running including the ones who waits on the queue
        if (shouldWaitForTasks != 0) {
            ret = pthread_mutex_lock(&(threadPool->empty_lock));
            if (ret != 0) {
                PRINT_ERROR;
                freeResources(threadPool);
                _exit(EXIT_FAILURE);
            }

            threadPool->should_wait = ShouldWait;

            ret = pthread_mutex_unlock(&(threadPool->empty_lock));
            if (ret != 0) {
                PRINT_ERROR;
                freeResources(threadPool);
                _exit(EXIT_FAILURE);
            }

            // wait just for the tasks that are already running
        } else {

            ret = pthread_mutex_lock(&(threadPool->empty_lock));
            if (ret != 0) {
                PRINT_ERROR;
                freeResources(threadPool);
                _exit(EXIT_FAILURE);
            }

            threadPool->should_wait = ShouldNotWait;

            ret = pthread_mutex_unlock(&(threadPool->empty_lock));
            if (ret != 0) {
                PRINT_ERROR;
                freeResources(threadPool);
                _exit(EXIT_FAILURE);
            }

        }

        int i;
        for (i = 0; i < threadPool->size; i++) {
            ret = pthread_join(threadPool->threads[i], NULL);
            if (ret != 0) {
                PRINT_ERROR;
                freeResources(threadPool);
                _exit(EXIT_FAILURE);
            }
        }

        // signal that the threadPool is desrtoyed
        threadPool->destroy_pool = 1;
        // free all the resources
        freeResources(threadPool);

    } else {
        return;
    }
}

// done
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param) {
    int ret;

    // if the user can add tasks
    if (threadPool->can_add_task && !(threadPool->destroy_pool)) {
        // initialize and create new task
        Task* new_task = malloc(sizeof(Task));
        if (new_task == NULL) {
            PRINT_ERROR;
            _exit(EXIT_FAILURE);
        }
        new_task->execute = computeFunc;
        new_task->params = param;

        ret = pthread_mutex_lock(&(threadPool->queue_lock));
        checkReturnValue(ret,threadPool);
        // add the new task to the queue
        osEnqueue(threadPool->queue, (void*)new_task);

        ret = pthread_mutex_unlock(&(threadPool->queue_lock));
        checkReturnValue(ret,threadPool);

        // signal that new task added
        ret = pthread_cond_signal(&(threadPool->wait_for_new_task));
        checkReturnValue(ret, threadPool);

        return 0;

        // if the user cannot add tasks
    } else {
        return -1;
    }
}