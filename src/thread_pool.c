// thread_pool.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://en.wikipedia.org/wiki/Thread_pool
//  - https://www.man7.org/linux/man-pages/man3/pthread_cond_init.3.html
//  - https://www.man7.org/linux/man-pages/man3/pthread_create.3.html
//  - https://www.man7.org/linux/man-pages/man3/pthread_mutex_lock.3.html

#include <errno.h>
#include <stdlib.h>
#include "thread_pool.h"
#define THREAD_POOL_CHUNK_SIZE 4096

#include <stdio.h>

static void* thread_pool_work(void* arg)
{
    ThreadPool instance = arg;
    struct TaskQueueNode result;

    while (task_queue_try_dequeue(&instance->tasks, &result))
    {
        printf("working on %p with size %zu...\n",
            (void*)result.buffer, result.size);
    }

    return NULL;
}

bool thread_pool(
    ThreadPool instance,
    MappedFileCollection mappedFiles,
    unsigned long workers)
{
    instance->threads = malloc(workers * sizeof * instance->threads);

    if (!instance->threads)
    {
        return false;
    }

    if (!task_queue(&instance->tasks))
    {
        free(instance->threads);

        return false;
    }

    if (!task_queue(&instance->completedTasks))
    {
        finalize_task_queue(&instance->tasks);
        free(instance->threads);

        return false;
    }

    for (int i = 0; i < mappedFiles->count; i++)
    {
        off_t chunks = mappedFiles->items[i].size / THREAD_POOL_CHUNK_SIZE;
        off_t remainder = mappedFiles->items[i].size % THREAD_POOL_CHUNK_SIZE;

        for (off_t chunk = 0; chunk < chunks; chunk++)
        {
            if (!task_queue_enqueue(
                &instance->tasks,
                mappedFiles->items[i].buffer + chunk * THREAD_POOL_CHUNK_SIZE,
                THREAD_POOL_CHUNK_SIZE))
            {
                finalize_task_queue(&instance->tasks);
                finalize_task_queue(&instance->completedTasks);
                free(instance->threads);

                return false;
            }
        }

        if (!task_queue_enqueue(
            &instance->tasks,
            mappedFiles->items[i].buffer + chunks * THREAD_POOL_CHUNK_SIZE,
            remainder))
        {
            finalize_task_queue(&instance->tasks);
            finalize_task_queue(&instance->completedTasks);
            free(instance->threads);

            return false;
        }
    }

    pthread_cond_init(&instance->empty, NULL);

    for (unsigned long i = 0; i < workers; i++)
    {
        int ex = pthread_create(
            instance->threads + i,
            NULL,
            thread_pool_work,
            instance);

        if (ex)
        {
            finalize_thread_pool(instance);

            for (unsigned long j = 0; j < i; j++)
            {
                pthread_join(instance->threads[j], NULL);
            }

            errno = ex;

            return false;
        }
    }

    for (unsigned long i = 0; i < workers; i++)
    {
        int ex = pthread_join(instance->threads[i], NULL);

        if (ex)
        {
            finalize_thread_pool(instance);

            errno = ex;

            return false;
        }
    }

    return true;
}

void finalize_thread_pool(ThreadPool instance)
{
    finalize_task_queue(&instance->tasks);
    finalize_task_queue(&instance->completedTasks);
    pthread_cond_destroy(&instance->empty);

    if (instance->threads)
    {
        free(instance->threads);

        instance->threads = NULL;
    }
}
