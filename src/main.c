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
    off_t taskIndex;
    off_t tasksCount;
    pthread_mutex_t mutex;
    pthread_cond_t notEmptyCondition;
    pthread_cond_t terminateCondition;
    struct Task* tasks;
    struct Task** completedTasks;
};

typedef struct ThreadPool* ThreadPool;

void finalize_thread_pool(ThreadPool instance)
{
    pthread_mutex_destroy(&instance->mutex);
    pthread_cond_destroy(&instance->notEmptyCondition);
    pthread_cond_destroy(&instance->terminateCondition);
    free(instance->tasks);
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

static void* thread_pool_consume(void* arg)
{
    ThreadPool pool = (ThreadPool)arg;
    int* result = calloc(1, sizeof * result);

    if (!result)
    {
        return NULL;
    }

    while (pool->taskIndex < pool->tasksCount)
    {
        while (!pool->tasksCount)
        {
            fprintf(stderr, "consumer: wait for task, taskIndex = %zu, tasksCount = %zu\n", pool->taskIndex, pool->tasksCount);

            int ex = pthread_cond_wait(&pool->notEmptyCondition, &pool->mutex);

            if (ex)
            {
                *result = ex;

                return result;
            }
        }

        pthread_mutex_lock(&pool->mutex);
        task_execute(pool->tasks + pool->taskIndex);

        pool->taskIndex++;

        pthread_mutex_unlock(&pool->mutex);
    }

    pthread_cond_signal(&pool->terminateCondition);

    return result;
}

static bool task_merge(struct Task tasks[], size_t count)
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

bool thread_pool(
    ThreadPool instance,
    MappedFileCollection mappedFiles,
    unsigned long jobs)
{
    int ex = pthread_mutex_init(&instance->mutex, NULL);

    if (ex)
    {
        errno = ex;

        return false;
    }

    ex = pthread_cond_init(&instance->notEmptyCondition, NULL);

    if (ex)
    {
        errno = ex;

        return false;
    }

    ex = pthread_cond_init(&instance->terminateCondition, NULL);

    if (ex)
    {
        pthread_cond_destroy(&instance->notEmptyCondition);

        errno = ex;

        return false;
    }

    instance->tasksCount = 0;
    instance->taskIndex = 0;
    instance->tasks = NULL;

    pthread_t* consumers = malloc(jobs * sizeof * consumers);

    if (!consumers)
    {
        pthread_mutex_destroy(&instance->mutex);
        pthread_cond_destroy(&instance->notEmptyCondition);
        pthread_cond_destroy(&instance->terminateCondition);

        return false;
    }

    for (unsigned long job = 0; job < jobs; job++)
    {
        ex = pthread_create(
            consumers + job,
            NULL,
            &thread_pool_consume,
            instance);

        if (ex)
        {
            errno = ex;

            free(consumers);
            pthread_mutex_destroy(&instance->mutex);
            pthread_cond_destroy(&instance->notEmptyCondition);
            pthread_cond_destroy(&instance->terminateCondition);

            return false;
        }
    }

    // void* result;

    // if (!result)
    // {
    //     free(consumers);
    //     pthread_mutex_destroy(&instance->mutex);
    //     pthread_cond_destroy(&instance->notEmptyCondition);

    //     return false;
    // }

    // ex = *(int*)result;

    // free(result);

    // if (ex)
    // {
    //     errno = ex;

    //     free(consumers);
    //     pthread_mutex_destroy(&instance->mutex);
    //     pthread_cond_destroy(&instance->notEmptyCondition);

    //     return false;
    // }

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
        pthread_mutex_destroy(&instance->mutex);
        pthread_cond_destroy(&instance->notEmptyCondition);
        pthread_cond_destroy(&instance->terminateCondition);

        return false;
    }

    size_t id = 0;

    for (int i = 0; i < mappedFiles->count; i++)
    {
        MappedFile mappedFile = mappedFiles->items[i];
        off_t offsets = mappedFile.size / TASK_SIZE;
        off_t remainder = mappedFile.size % TASK_SIZE;

        for (off_t offset = 0; offset < offsets; offset++)
        {
            tasks[id].id = id;
            tasks[id].input = mappedFile.buffer + offset * TASK_SIZE;
            tasks[id].inputSize = TASK_SIZE;
            id++;
        }

        if (remainder)
        {
            tasks[id].id = id;
            tasks[id].input = mappedFile.buffer + offsets * TASK_SIZE;
            tasks[id].inputSize = remainder;
            id++;
        }
    }

    pthread_mutex_lock(&instance->mutex);

    instance->tasksCount = tasksCount;
    instance->tasks = tasks;

    pthread_mutex_unlock(&instance->mutex);
    pthread_cond_signal(&instance->notEmptyCondition);

    // for (unsigned long job = 0; job < jobs; job++)
    // {
    //     void* result;

    //     ex = pthread_join(consumers[job], &result);

    //     if (ex)
    //     {
    //         errno = ex;

    //         free(consumers);
    //         free(instance->tasks);
    //         pthread_mutex_destroy(&instance->mutex);
    //         pthread_cond_destroy(&instance->notEmptyCondition);
    //         pthread_cond_destroy(&instance->terminateCondition);

    //         return false;
    //     }

    //     if (!result)
    //     {
    //         free(consumers);
    //         free(instance->tasks);
    //         pthread_mutex_destroy(&instance->mutex);
    //         pthread_cond_destroy(&instance->notEmptyCondition);
    //         pthread_cond_destroy(&instance->terminateCondition);

    //         return false;
    //     }

    //     ex = *(int*)result;

    //     free(result);

    //     if (ex)
    //     {
    //         errno = ex;

    //         free(consumers);
    //         free(instance->tasks);
    //         pthread_mutex_destroy(&instance->mutex);
    //         pthread_cond_destroy(&instance->notEmptyCondition);
    //         pthread_cond_destroy(&instance->terminateCondition);

    //         return false;
    //     }
    // }

    while (instance->taskIndex < instance->tasksCount)
    {
        ex = pthread_cond_wait(&instance->terminateCondition, &instance->mutex);

        if (ex)
        {
            errno = ex;

            return false;
        }
    }

    free(consumers);

    ex = task_merge(instance->tasks, instance->tasksCount);

    free(instance->tasks);
    pthread_mutex_destroy(&instance->mutex);
    pthread_cond_destroy(&instance->notEmptyCondition);
    pthread_cond_destroy(&instance->terminateCondition);

    return ex;
}

static bool main_encode_parallel(
    MappedFileCollection mappedFiles,
    unsigned long jobs)
{
    struct ThreadPool pool;

    if (!thread_pool(&pool, mappedFiles, jobs))
    {
        return false;
    }

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
