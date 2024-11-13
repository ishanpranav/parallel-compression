// thread_pool.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://www.man7.org/linux/man-pages/man3/pthread_mutex_init.3p.html

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include "thread_pool.h"

bool thread_pool(ThreadPool instance, MappedFileCollection mappedFiles)
{
    size_t count = 0;

    for (int i = 0; i < mappedFiles->count; i++)
    {
        count += mappedFiles->items[i].size / TASK_SIZE;

        if (mappedFiles->items[i].size % TASK_SIZE != 0)
        {
            count++;
        }
    }

    struct Task* items = malloc(count * sizeof * items);

    assert(items);

    if (!items)
    {
        return false;
    }

    instance->items = items;
    instance->count = 0;
    instance->index = 0;
    instance->resultId = 0;
    instance->flushId = 0;
    instance->resultsCount = 0;

    int ex = pthread_mutex_init(&instance->mutex, NULL);

    assert(!ex);

    if (ex)
    {
        free(items);

        errno = ex;

        return false;
    }

    ex = pthread_cond_init(&instance->producer, NULL);

    assert(!ex);

    if (ex)
    {
        free(items);
        pthread_mutex_destroy(&instance->mutex);

        errno = ex;

        return false;
    }

    ex = pthread_cond_init(&instance->consumer, NULL);

    assert(!ex);
    
    if (ex)
    {
        free(items);
        pthread_mutex_destroy(&instance->mutex);
        pthread_cond_destroy(&instance->producer);

        errno = ex;

        return false;
    }

    return true;
}

bool thread_pool_dequeue(ThreadPool instance, Task* result)
{
    pthread_mutex_lock(&instance->mutex);

    while (!instance->count)
    {
        pthread_cond_wait(&instance->producer, &instance->mutex);
    }

    if (instance->index >= instance->count)
    {
        instance->resultId = instance->resultsCount;

        pthread_cond_signal(&instance->consumer);
        pthread_mutex_unlock(&instance->mutex);

        return false;
    }

    *result = instance->items + instance->index;
    instance->index++;

    pthread_mutex_unlock(&instance->mutex);

    return true;
}

void finalize_thread_pool(ThreadPool instance)
{
    instance->count = 0;
    instance->index = 0;

    free(instance->items);
    pthread_mutex_destroy(&instance->mutex);
    pthread_cond_destroy(&instance->producer);
    pthread_cond_destroy(&instance->consumer);
}
