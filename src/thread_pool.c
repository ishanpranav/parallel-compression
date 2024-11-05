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

        memcpy(output + outputSize, &encoder, sizeof encoder);

        outputSize += sizeof encoder;

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

    for (int i = 0; i < count; i++) 
    {
        if (fwrite(
            tasks[i].buffer,
            sizeof * tasks[i].buffer,
            tasks[i].size,
            stdout) != (size_t)tasks[i].size)
        {
            finalize_thread_pool(instance);

            return false;
        }

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
