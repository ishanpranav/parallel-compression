// main.c
// Copyright (c) 2024 Ishan Pranav
// Licensed under the MIT license.

// References:
//  - https://www.man7.org/linux/man-pages/man3/getopt.3.html
//  - https://www.man7.org/linux/man-pages/man3/perror.3.html
//  - https://www.man7.org/linux/man-pages/man3/sprintf.3p.html
//  - https://www.man7.org/linux/man-pages/man3/strtol.3.html

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "encoder.h"
#include "mapped_file_collection.h"
#define TASK_SIZE 4096
// #include "thread_pool.h"

static void main_print_usage(FILE* output, char* args[])
{
    fprintf(output, "Usage: %s [OPTION]... FILE...\n", args[0]);
}

static bool main_encode_sequential(MappedFileCollection mappedFiles)
{
    Encoder encoder = { 0 };

    for (int i = 0; i < mappedFiles->count; i++)
    {
        if (!encoder_next_encode(&encoder, mappedFiles->items[i]))
        {
            return false;
        }
    }

    encoder_end_encode(encoder);

    return true;
}

struct Task
{
    off_t inputSize;
    off_t outputSize;
    size_t id;
    unsigned char* input;
    unsigned char output[TASK_SIZE * 2];
};

typedef struct Task* Task;

struct TaskQueue
{
    size_t count;
    size_t index;
    pthread_mutex_t mutex;
    pthread_cond_t producer;
    pthread_cond_t consumer;
    struct Task* items;
};

struct ThreadPool
{
    size_t resultId;
    size_t resultsCount;
    struct TaskQueue tasks;
};

typedef struct TaskQueue* TaskQueue;
typedef struct ThreadPool* ThreadPool;

static void task_queue(TaskQueue instance, MappedFileCollection mappedFiles)
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

    instance->items = items;
    instance->count = 0;
    instance->index = 0;

    pthread_mutex_init(&instance->mutex, NULL);
    pthread_cond_init(&instance->producer, NULL);
    pthread_cond_init(&instance->consumer, NULL);
}

static void task_execute(Task instance)
{
    off_t outputSize = 0;
    Encoder encoder = { 0 };

    for (off_t i = 0; i < instance->inputSize; i++)
    {
        unsigned char current = instance->input[i];

        if (!encoder.count)
        {
            encoder.previous = current;
            encoder.count = 1;

            continue;
        }

        if (current == encoder.previous && encoder.count < UCHAR_MAX)
        {
            encoder.count++;

            continue;
        }

        memcpy(instance->output + outputSize, &encoder, sizeof encoder);

        outputSize += sizeof encoder;
        encoder.count = 1;
        encoder.previous = current;
    }

    if (encoder.count)
    {
        memcpy(instance->output + outputSize, &encoder, sizeof encoder);

        outputSize += sizeof encoder;
    }

    instance->outputSize = outputSize;
}

bool thread_pool(ThreadPool instance, MappedFileCollection mappedFiles)
{
    task_queue(&instance->tasks, mappedFiles);
    
    instance->resultId = 0;
    instance->resultsCount = 0;

    return true;
}

bool task_queue_dequeue(TaskQueue instance, Task* result)
{
    pthread_mutex_lock(&instance->mutex);

    while (!instance->count)
    {
        fprintf(stderr, "wait thread\n");
        pthread_cond_wait(&instance->producer, &instance->mutex);
    }

    if (instance->index >= instance->count)
    {
        fprintf(stderr, "index %zu count %zu\n", instance->index, instance->count);
        pthread_mutex_unlock(&instance->mutex);

        return false;
    }

    *result = instance->items + instance->index;
    instance->index++;

    pthread_mutex_unlock(&instance->mutex);

    return true;
}

static void* main_consume(void* arg)
{
    ThreadPool pool = (ThreadPool)arg;
    Task current;

    while (task_queue_dequeue(&pool->tasks, &current))
    {
        task_execute(current);
        pthread_mutex_lock(&pool->tasks.mutex);

        pool->resultsCount++;

        if (pool->resultId == current->id)
        {
            pool->resultId++;

            pthread_cond_signal(&pool->tasks.consumer);
        }

        pthread_mutex_unlock(&pool->tasks.mutex);
    }

    pthread_cond_signal(&pool->tasks.consumer);

    fprintf(stderr, "exit thread\n");
    return NULL;
}

static void main_produce(
    ThreadPool pool,
    MappedFileCollection mappedFiles,
    unsigned long jobs)
{
    pthread_mutex_lock(&pool->tasks.mutex);

    size_t id = 0;

    for (int i = 0; i < mappedFiles->count; i++)
    {
        MappedFile mappedFile = mappedFiles->items[i];
        off_t offsets = mappedFile.size / TASK_SIZE;
        off_t remainder = mappedFile.size % TASK_SIZE;
        struct Task* items = pool->tasks.items;

        for (off_t offset = 0; offset < offsets; offset++)
        {
            items[id].id = id;
            items[id].input = mappedFile.buffer + offset * TASK_SIZE;
            items[id].inputSize = TASK_SIZE;
            id++;
        }

        if (remainder)
        {
            items[id].id = id;
            items[id].input = mappedFile.buffer + offsets * TASK_SIZE;
            items[id].inputSize = remainder;
            id++;
        }
    }

    pool->tasks.count = id;

    for (unsigned long job = 0; job < jobs; job++)
    {
        pthread_cond_signal(&pool->tasks.producer);
    }

    pthread_mutex_unlock(&pool->tasks.mutex);

    for (;;)
    {
        pthread_mutex_lock(&pool->tasks.mutex);
        pthread_cond_wait(&pool->tasks.consumer, &pool->tasks.mutex);
        
        fprintf(stderr, "resultId: %zu, resultscount %zu > %zu\n", pool->resultId, pool->resultsCount, pool->tasks.count);
        
        if (pool->resultsCount >= pool->tasks.count)
        {
            pthread_mutex_unlock(&pool->tasks.mutex);

            break;
        }

        pthread_mutex_unlock(&pool->tasks.mutex);
    }
}

void finalize_task_queue(TaskQueue instance)
{
    instance->count = 0;
    instance->index = 0;

    free(instance->items);
    pthread_mutex_destroy(&instance->mutex);
    pthread_cond_destroy(&instance->producer);
    pthread_cond_destroy(&instance->consumer);
}

void finalize_thread_pool(ThreadPool instance)
{
    finalize_task_queue(&instance->tasks);
}

static bool main_merge(struct Task tasks[], size_t count)
{
    bool merging = false;

    for (size_t i = 0; i < count; i++)
    {
        // fprintf(stderr, "for %zu: ", i);
        // fprintf(stderr, "in %zu, out %zu\n", tasks[i].inputSize, tasks[i].outputSize);

        unsigned char* buffer = tasks[i].output;
        size_t size = tasks[i].outputSize;

        if (merging)
        {
            buffer += 2;
            size -= 2;
            merging = false;
        }

        if (i < count - 1 && size >= 2 && tasks[i + 1].outputSize >= 2)
        {
            unsigned char current = buffer[size - 2];
            unsigned int count = buffer[size - 1];
            unsigned char next = tasks[i + 1].output[0];
            unsigned int nextCount = tasks[i + 1].output[1];

            if (current == next && count + nextCount <= UCHAR_MAX)
            {
                buffer[size - 1] += nextCount;
                merging = true;
            }
        }

        if (fwrite(buffer, sizeof * buffer, size, stdout) != size)
        {
            return false;
        }
    }

    return true;
}

static bool main_encode_parallel(
    MappedFileCollection mappedFiles,
    unsigned long jobs)
{
    struct ThreadPool pool;

    if (!thread_pool(&pool, mappedFiles))
    {
        return false;
    }

    pthread_t* consumers = malloc(jobs * sizeof * consumers);

    for (unsigned long job = 0; job < jobs; job++)
    {
        pthread_create(consumers + job, NULL, main_consume, &pool);
    }

    main_produce(&pool, mappedFiles, jobs);

    for (unsigned long job = 0; job < jobs; job++)
    {
        pthread_join(consumers[job], NULL);
    }

    free(consumers);
    fprintf(stderr, "freed consumers\n");

    // fprintf(stderr, "i have %zu items at index %zu\n", pool.tasks.count, pool.tasks.index);

    main_merge(pool.tasks.items, pool.tasks.count);
    finalize_thread_pool(&pool);

    return true;
}

int main(int count, char* args[])
{
    int option;
    unsigned long jobs = 1;

    while ((option = getopt(count, args, "hj:")) != -1)
    {
        switch (option)
        {
        case 'h':
            main_print_usage(stdout, args);

            return EXIT_SUCCESS;

        case 'j':
            errno = 0;
            jobs = strtoul(optarg, NULL, 10);

            if (errno || jobs < 1)
            {
                main_print_usage(stderr, args);

                return EXIT_FAILURE;
            }
            break;

        default: return EXIT_FAILURE;
        }
    }

    if (optind >= count)
    {
        main_print_usage(stderr, args);

        return EXIT_FAILURE;
    }

    struct MappedFileCollection mappedFiles;
    int fileCount = count - optind;
    int ex = mapped_file_collection(&mappedFiles, args + optind, fileCount);
    char* app = args[0];

    if (ex == -1)
    {
        perror(app);

        return EXIT_FAILURE;
    }

    if (ex < fileCount)
    {
        char* path = args[optind + ex];

        fprintf(stderr, "%s: %s: %s\n", app, path, strerror(errno));

        return EXIT_FAILURE;
    }

    bool result;

    if (jobs == 1)
    {
        result = main_encode_sequential(&mappedFiles);
    }
    else
    {
        result = main_encode_parallel(&mappedFiles, jobs);
    }

    finalize_mapped_file_collection(&mappedFiles);

    if (!result)
    {
        perror(app);

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
