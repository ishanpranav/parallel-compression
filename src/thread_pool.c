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
#include <string.h>
#include "encoder.h"
#include "thread_pool.h"
#define THREAD_POOL_CHUNK_SIZE 4096

#include <stdio.h>

static void* thread_pool_work(void* arg)
{
    ThreadPool instance = arg;
    struct TaskQueueNode task;

    while (task_queue_try_dequeue(&instance->tasks, &task))
    {
        Encoder encoder = { 0 };
        unsigned char* output = malloc(2 * task.size * sizeof * output);
        off_t outputSize = encoder_encode(
            output,
            &encoder,
            task.buffer,
            task.size);

        task_queue_enqueue(
            &instance->completedTasks,
            task.order,
            output,
            outputSize);
    }

    return NULL;
}

int compare_task(const void* p, const void* q)
{
    const struct TaskQueueNode* a = (const struct TaskQueueNode*)p;
    const struct TaskQueueNode* b = (const struct TaskQueueNode*)q;

    return a->order - b->order;
}

#include <limits.h>
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

    off_t order = 1;

    for (int i = 0; i < mappedFiles->count; i++)
    {
        off_t chunks = mappedFiles->items[i].size / THREAD_POOL_CHUNK_SIZE;
        off_t remainder = mappedFiles->items[i].size % THREAD_POOL_CHUNK_SIZE;

        for (off_t chunk = 0; chunk < chunks; chunk++)
        {
            if (!task_queue_enqueue(
                &instance->tasks,
                order,
                mappedFiles->items[i].buffer + chunk * THREAD_POOL_CHUNK_SIZE,
                THREAD_POOL_CHUNK_SIZE))
            {
                finalize_task_queue(&instance->tasks);
                finalize_task_queue(&instance->completedTasks);
                free(instance->threads);

                return false;
            }
            
            order++;
        }
        
        if (!task_queue_enqueue(
            &instance->tasks,
            order,
            mappedFiles->items[i].buffer + chunks * THREAD_POOL_CHUNK_SIZE,
            remainder))
        {
            finalize_task_queue(&instance->tasks);
            finalize_task_queue(&instance->completedTasks);
            free(instance->threads);

            return false;
        }
        
        order++;
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

    struct TaskQueueNode* tasks = malloc(10000 * sizeof * tasks);
    int count = 0;
    struct TaskQueueNode completedTask;

    while (task_queue_try_dequeue(&instance->completedTasks, &completedTask))
    {
        tasks[count] = completedTask;
        count++;
    }

    qsort(tasks, count, sizeof * tasks, compare_task);
    fprintf(stderr, "%d packets arrived\n", count);

    // bool lastSkip = false;
    bool nextSkip = false;

    for (int i = 0; i < count; i++) 
    {
        fprintf(stderr, "packet %d with order %zu\n", i, tasks[i].order);

        unsigned char* buffer = tasks[i].buffer;
        size_t size = tasks[i].size;

        if (nextSkip)
        {
            buffer += 2;
            size -= 2;    
            nextSkip = false;
        }

        if (i < count - 1)
        {
            unsigned char current = buffer[size - 2];
            unsigned int count = buffer[size - 1]; 
            unsigned char next = tasks[i + 1].buffer[0];
            unsigned int nextCount = tasks[i + 1].buffer[1];

            if (current == next && count + nextCount <= UINT_MAX)
            {
                buffer[size - 1] += nextCount;
                nextSkip = true;
            }   
        }

        if (fwrite(buffer, sizeof * buffer, size, stdout) != size)
        {
            finalize_thread_pool(instance);

            return false;
        }
    }

    for (int i = 0; i < count; i++)
    {
        free(tasks[i].buffer);
    }

    free(tasks);

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
