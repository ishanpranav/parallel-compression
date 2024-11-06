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
#include "task.h"
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

struct ThreadPool
{
    bool empty;
    off_t taskIndex;
    off_t tasksCount;
    pthread_mutex_t mutex;
    pthread_cond_t emptyCondition;
    pthread_cond_t fullCondition;
    struct Task* tasks;
};

typedef struct ThreadPool* ThreadPool;

bool thread_pool(ThreadPool instance, MappedFileCollection mappedFiles)
{
    int ex = pthread_mutex_init(&instance->mutex, NULL);

    if (ex)
    {
        errno = ex;

        return false;
    }

    ex = pthread_cond_init(&instance->emptyCondition, NULL);

    if (ex)
    {
        errno = ex;

        return false;
    }

    ex = pthread_cond_init(&instance->fullCondition, NULL);

    if (ex)
    {
        errno = ex;

        pthread_cond_destroy(&instance->emptyCondition);

        return false;
    }

    size_t tasksCount = 0;

    for (int i = 0; i < mappedFiles->count; i++)
    {
        tasksCount += mappedFiles->items[i].size / TASK_SIZE;

        if (mappedFiles->items[i].size % TASK_SIZE != 0)
        {
            tasksCount++;
        }
    }

    struct Task* tasks = malloc(tasksCount * sizeof * tasks);

    if (!tasks)
    {
        pthread_cond_destroy(&instance->emptyCondition);
        pthread_cond_destroy(&instance->fullCondition);

        return false;
    }

    size_t j = 0;

    for (int i = 0; i < mappedFiles->count; i++)
    {
        MappedFile mappedFile = mappedFiles->items[i];
        off_t offsets = mappedFile.size / TASK_SIZE;
        off_t remainder = mappedFile.size % TASK_SIZE;

        for (off_t offset = 0; offset < offsets; offset++)
        {
            tasks[j].input = mappedFile.buffer + offset * TASK_SIZE;
            tasks[j].inputSize = TASK_SIZE;
            j++;
        }

        if (remainder)
        {
            tasks[j].input = mappedFile.buffer + offsets * TASK_SIZE;
            tasks[j].inputSize = remainder;
            j++;
        }
    }

    fprintf(stderr, "j is %zu, tasksCount is %zu\n", j, tasksCount);

    instance->tasks = tasks;
    instance->tasksCount = tasksCount;
    instance->taskIndex = -1;
    instance->empty = true;

    return true;
}

void finalize_thread_pool(ThreadPool instance)
{
    pthread_mutex_destroy(&instance->mutex);
    pthread_cond_destroy(&instance->emptyCondition);
    pthread_cond_destroy(&instance->fullCondition);
    free(instance->tasks);
}

static void* main_produce(void* arg)
{
    ThreadPool pool = (ThreadPool)arg;
    int* result = calloc(1, sizeof * result);

    if (!result)
    {
        return NULL;
    }

    for (;;)
    {
        fprintf(stderr, "producer: lock\n");

        int ex = pthread_mutex_lock(&pool->mutex);

        if (ex)
        {
            *result = ex;
            
            return result;
        }

        while (!pool->empty)
        {
            fprintf(stderr, "producer: wait until empty\n");

            ex = pthread_cond_wait(&pool->emptyCondition, &pool->mutex);

            if (ex)
            {
                *result = ex;

                return result;
            }
        }

        if (pool->taskIndex == pool->tasksCount)
        {
            fprintf(stderr, "producer: unlock, exhausted\n");

            ex = pthread_mutex_unlock(&pool->mutex);
            *result = ex;

            return result;
        }
        
        fprintf(stderr, "producer: signal full\n");

        pool->taskIndex++;
        pool->empty = false;
        ex = pthread_cond_signal(&pool->fullCondition);

        if (ex)
        {
            *result = ex;

            return result;
        }

        fprintf(stderr, "producer: unlock\n");
        
        ex = pthread_mutex_unlock(&pool->mutex);

        if (ex)
        {
            *result = ex;

            return result;
        }
    }
}

void task_execute(Task instance)
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

static void* main_consume(void* arg)
{
    ThreadPool pool = (ThreadPool)arg;
    int* result = calloc(1, sizeof * result);

    if (!result)
    {
        return NULL;
    }

    for (;;)
    {
        fprintf(stderr, "consumer: lock\n");

        int ex = pthread_mutex_lock(&pool->mutex);

        if (ex)
        {
            *result = ex;

            return result;
        }

        while (pool->empty && pool->taskIndex < pool->tasksCount)
        {
            fprintf(stderr, "consumer: wait until full\n");
        
            ex = pthread_cond_wait(&pool->fullCondition, &pool->mutex);

            if (ex)
            {
                *result = ex;

                return result;
            }
        }

        fprintf(stderr, "consumer: signal empty\n");

        pool->empty = true;
        ex = pthread_cond_signal(&pool->emptyCondition);

        if (ex)
        {
            *result = ex;

            return result;
        }

        fprintf(stderr, "consumer: unlock\n");

        ex = pthread_mutex_unlock(&pool->mutex);

        if (ex)
        {
            *result = ex;

            return result;
        }

        if (pool->taskIndex == pool->tasksCount)
        {
            fprintf(stderr, "consumer: stop, exhausted\n");
            
            return result;
        }
        
        task_execute(pool->tasks + pool->taskIndex);
    }
}

bool task_merge(struct Task tasks[], size_t count)
{
    bool merging = false;

    for (size_t i = 0; i < count; i++)
    {
        fprintf(stderr, "for %zu: ", i);
        fprintf(stderr, "in %zu, out %zu\n", tasks[i].inputSize, tasks[i].outputSize);

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

    pthread_t producer;
    int ex = pthread_create(&producer, NULL, &main_produce, &pool);

    if (ex)
    {
        errno = ex;

        finalize_thread_pool(&pool);

        return false;
    }

    pthread_t* consumers = malloc(jobs * sizeof * consumers);

    if (!consumers)
    {
        finalize_thread_pool(&pool);

        return false;
    }

    void* result;

    for (unsigned long job = 0; job < jobs; job++)
    {
        ex = pthread_create(consumers + job, NULL, &main_consume, &pool);

        if (ex)
        {
            errno = ex;

            free(consumers);
            finalize_thread_pool(&pool);

            return false;
        }
    }

    ex = pthread_join(producer, &result);

    if (ex)
    {
        errno = ex;

        free(consumers);
        finalize_thread_pool(&pool);

        return false;
    }

    if (!result)
    {
        free(consumers);
        finalize_thread_pool(&pool);

        return false;
    }

    ex = *(int*)result;

    free(result);

    if (ex)
    {
        errno = ex;

        free(consumers);
        finalize_thread_pool(&pool);

        return false;
    }

    for (unsigned long job = 0; job < jobs; job++)
    {
        ex = pthread_join(consumers[job], &result);

        if (ex)
        {
            errno = ex;

            free(consumers);
            finalize_thread_pool(&pool);

            return false;
        }

        if (!result)
        {
            free(consumers);
            finalize_thread_pool(&pool);

            return false;
        }

        ex = *(int*)result;

        free(result);

        if (ex)
        {
            errno = ex;

            free(consumers);
            finalize_thread_pool(&pool);

            return false;
        }
    }

    free(consumers);

    fprintf(stderr, "there are %zu tasks, index is %zu\n", pool.tasksCount, pool.taskIndex);
    
    ex = task_merge(pool.tasks, pool.tasksCount);
    
    finalize_thread_pool(&pool);
    
    return ex;
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
